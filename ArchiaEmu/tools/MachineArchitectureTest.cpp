#include "core/Bus.hpp"
#include "core/Device.hpp"
#include "core/Machine.hpp"
#include "core/MmioRegisterDevice.hpp"
#include "core/RamDevice.hpp"
#include "core/RomDevice.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace myps5emu;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    Machine machine;
    Bus& bus = machine.SystemBus();

    if (machine.CPU().InstructionPointer() != 0) {
        return Fail("Machine CPU initialization failed") ? 0 : 1;
    }

    if (!bus.Map(0x400000, 0x1000,
                  MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Machine RAM mapping failed") ? 0 : 1;
    }

    if (bus.HasPermissionAt(0x400000, 1, MemoryPermission::Execute) ||
        !bus.HasPermissionAt(0x400000, 1, MemoryPermission::Read) ||
        !bus.HasPermissionAt(0x400000, 1, MemoryPermission::Write)) {
        return Fail("RAM permissions were not applied") ? 0 : 1;
    }

    if (bus.Map(0x410001, 0x1000) ||
        bus.LastFault() != MemoryFault::Unaligned ||
        bus.Map(0x420000, 0x1001) ||
        bus.LastFault() != MemoryFault::Unaligned) {
        return Fail("Memory accepted non-page-aligned mapping") ? 0 : 1;
    }

    std::uint8_t faultByte = 0;
    if (bus.Read(0x600000, &faultByte, 1) ||
        bus.LastFault() != MemoryFault::Unmapped) {
        return Fail("Unmapped access did not report a fault") ? 0 : 1;
    }

    if (!bus.Map(0x430000, 0x1000,
                  MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Executable page mapping failed") ? 0 : 1;
    }

    const std::uint8_t ramWrite[] = {0x42, 0x43};
    if (!bus.Write(0x400000, ramWrite, sizeof(ramWrite))) {
        return Fail("Machine RAM write failed") ? 0 : 1;
    }

    std::uint8_t ramRead[sizeof(ramWrite)] = {};
    if (!bus.Read(0x400000, ramRead, sizeof(ramRead)) ||
        ramRead[0] != 0x42 || ramRead[1] != 0x43) {
        return Fail("Machine RAM read failed") ? 0 : 1;
    }

    RamDevice ramDevice(0x20);
    if (ramDevice.Size() != 0x20) {
        return Fail("RAM device size failed") ? 0 : 1;
    }

    const std::uint8_t deviceWrite[] = {0x10, 0x20, 0x30, 0x40};
    if (!ramDevice.Write(4, deviceWrite, sizeof(deviceWrite))) {
        return Fail("RAM device write failed") ? 0 : 1;
    }

    std::uint8_t deviceRead[sizeof(deviceWrite)] = {};
    if (!ramDevice.Read(4, deviceRead, sizeof(deviceRead)) ||
        deviceRead[0] != 0x10 || deviceRead[3] != 0x40) {
        return Fail("RAM device read failed") ? 0 : 1;
    }

    if (ramDevice.Read(0x20, deviceRead, 1) ||
        ramDevice.Write(0x1F, deviceRead, 2)) {
        return Fail("RAM device accepted out-of-range access") ? 0 : 1;
    }

    if (ramDevice.Read(0, nullptr, 1) ||
        ramDevice.Write(0, nullptr, 1)) {
        return Fail("RAM device accepted null buffer") ? 0 : 1;
    }

    RomDevice rom({0xAA, 0xBB, 0xCC, 0xDD});
    if (rom.Size() != 4) {
        return Fail("ROM device size failed") ? 0 : 1;
    }

    std::uint8_t romRead[2] = {};
    if (!rom.Read(1, romRead, sizeof(romRead)) ||
        romRead[0] != 0xBB || romRead[1] != 0xCC) {
        return Fail("ROM device read failed") ? 0 : 1;
    }

    const std::uint8_t romWrite = 0xFF;
    if (rom.Write(0, &romWrite, 1) ||
        rom.Read(3, romRead, 2)) {
        return Fail("ROM device accepted invalid access") ? 0 : 1;
    }

    MmioRegisterDevice registers(
        {0x11223344U, 0xAABBCCDDU},
        {0x0000FFFFU, 0xFFFFFFFFU});

    if (registers.Size() != 8 ||
        registers.ReadRegister(0) != 0x11223344U) {
        return Fail("MMIO register initialization failed") ? 0 : 1;
    }

    std::uint8_t registerBytes[4] = {};
    if (!registers.Read(0, registerBytes, sizeof(registerBytes)) ||
        registerBytes[0] != 0x44 || registerBytes[1] != 0x33 ||
        registerBytes[2] != 0x22 || registerBytes[3] != 0x11) {
        return Fail("MMIO register little-endian read failed") ? 0 : 1;
    }

    const std::uint8_t registerWrite[] = {0xCC, 0xDD, 0xEE, 0xFF};
    if (!registers.Write(0, registerWrite, sizeof(registerWrite)) ||
        registers.ReadRegister(0) != 0x1122DDCCU) {
        return Fail("MMIO register write mask failed") ? 0 : 1;
    }

    if (registers.Read(2, registerBytes, sizeof(registerBytes)) ||
        registers.Write(1, registerBytes, sizeof(registerBytes)) ||
        registers.Read(8, registerBytes, sizeof(registerBytes))) {
        return Fail("MMIO register accepted invalid access") ? 0 : 1;
    }

    if (!bus.MapDevice(0x10000000, 0x20, &ramDevice)) {
        return Fail("Device mapping failed") ? 0 : 1;
    }

    if (!bus.IsMapped(0x10000000, 1) ||
        !bus.HasPermissionAt(0x10000000, 1, MemoryPermission::Read) ||
        bus.HasPermissionAt(0x10000000, 1, MemoryPermission::Execute) ||
        !bus.Write(0x10000004, deviceWrite, sizeof(deviceWrite)) ||
        !bus.Read(0x10000004, deviceRead, sizeof(deviceRead)) ||
        deviceRead[0] != 0x10 || deviceRead[3] != 0x40) {
        return Fail("Bus device offset routing failed") ? 0 : 1;
    }

    if (bus.Read(0x1000001F, deviceRead, 2) ||
        bus.Write(0x1000001F, deviceWrite, 2)) {
        return Fail("Bus accepted access crossing device boundary") ? 0 : 1;
    }

    if (!bus.MapDevice(0x20000000, registers.Size(), &registers) ||
        !bus.Write(0x20000000, registerWrite, sizeof(registerWrite)) ||
        registers.ReadRegister(0) != 0x1122DDCCU) {
        return Fail("Bus MMIO register routing failed") ? 0 : 1;
    }

    if (bus.MapDevice(0x10000010, 0x10, &ramDevice) ||
        bus.MapDevice(0x400000, 0x10, &ramDevice) ||
        bus.Map(0x10000010, 0x10)) {
        return Fail("Bus accepted overlapping mapping") ? 0 : 1;
    }

    if (bus.Map(0x500000, 0, MemoryPermission::Read) ||
        bus.LastFault() != MemoryFault::InvalidRange) {
        return Fail("Bus::Map zero size did not report InvalidRange") ? 0 : 1;
    }

    if (bus.MapDevice(0x30000000, 0, &ramDevice) ||
        bus.LastFault() != MemoryFault::InvalidRange) {
        return Fail("Bus did not report invalid device mapping range") ? 0 : 1;
    }

    if (bus.MapDevice(0x20000000, 0x10, &ramDevice) ||
        bus.LastFault() != MemoryFault::Overlap) {
        return Fail("Bus did not report overlapping device mapping") ? 0 : 1;
    }

    if (bus.Map(0x400000, 0x1000) ||
        bus.LastFault() != MemoryFault::Overlap) {
        return Fail("Bus did not report overlapping RAM mapping") ? 0 : 1;
    }

    if (!bus.UnmapDevice(&ramDevice) ||
        bus.UnmapDevice(&ramDevice)) {
        return Fail("Device unmapping semantics failed") ? 0 : 1;
    }

    if (bus.Read(0x10000000, deviceRead, 1)) {
        return Fail("Unmapped device remained accessible") ? 0 : 1;
    }

    const std::uint8_t program[] = {
        0xB8, 0x78, 0x56, 0x34, 0x12,
        0xF4
    };

    if (!bus.Write(0x430000, program, sizeof(program)) ||
        !bus.Protect(0x430000, 0x1000,
                     MemoryPermission::Read | MemoryPermission::Execute)) {
        return Fail("Executable page protection transition failed") ? 0 : 1;
    }

    if (bus.Write(0x430000, program, sizeof(program)) ||
        bus.LastFault() != MemoryFault::PermissionDenied) {
        return Fail("RX write did not report a permission fault") ? 0 : 1;
    }

    machine.CPU().SetInstructionPointer(0x430000);
    if (machine.CPU().Run() != 0 ||
        machine.CPU().ReadRegister64(0) != 0x12345678ULL) {
        return Fail("CPU did not execute through executable page") ? 0 : 1;
    }

    if (!bus.Map(0x440000, 0x1000,
                  MemoryPermission::Read | MemoryPermission::Write) ||
        !bus.Write(0x440000, program, sizeof(program)) ||
        !bus.Protect(0x440000, 0x1000,
                     MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("NX page setup failed") ? 0 : 1;
    }

    MemoryFault observedFault = MemoryFault::None;
    CpuExceptionKind observedException = CpuExceptionKind::None;
    std::uint64_t observedFaultIp = 0;
    bool exceptionObserved = false;
    machine.CPU().SetExceptionHandler(
        [&](Cpu&, const CpuException& exception) {
            observedException = exception.kind;
            observedFault = exception.memory_fault;
            observedFaultIp = exception.instruction_pointer;
            exceptionObserved = true;
            return false;
        });

    machine.CPU().SetInstructionPointer(0x440000);
    if (machine.CPU().Run() == 0 ||
        bus.LastFault() != MemoryFault::PermissionDenied ||
        machine.CPU().LastMemoryFault() != MemoryFault::PermissionDenied ||
        !exceptionObserved ||
        observedException != CpuExceptionKind::MemoryFault ||
        observedFault != MemoryFault::PermissionDenied ||
        observedFaultIp != 0x440000 ||
        machine.CPU().LastException().kind != CpuExceptionKind::MemoryFault ||
        machine.CPU().LastException().instruction_pointer != 0x440000 ||
        machine.CPU().LastException().vector != CpuExceptionVector::PageFault) {
        return Fail("CPU memory exception dispatch failed") ? 0 : 1;
    }



    // #UD / UD2: invalid opcode must be reported at the start of the
    // faulting instruction, without advancing the saved exception IP.
    if (!bus.Map(0x450000, 0x1000,
                 MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Invalid-opcode page mapping failed") ? 0 : 1;
    }

    const std::uint8_t ud2Program[] = {0x0F, 0x0B, 0xF4};
    if (!bus.Write(0x450000, ud2Program, sizeof(ud2Program)) ||
        !bus.Protect(0x450000, 0x1000,
                     MemoryPermission::Read | MemoryPermission::Execute)) {
        return Fail("Invalid-opcode page setup failed") ? 0 : 1;
    }

    observedException = CpuExceptionKind::None;
    observedFault = MemoryFault::None;
    observedFaultIp = 0;
    exceptionObserved = false;

    machine.CPU().SetInstructionPointer(0x450000);
    if (machine.CPU().Run() == 0 ||
        !exceptionObserved ||
        observedException != CpuExceptionKind::InvalidOpcode ||
        observedFault != MemoryFault::None ||
        observedFaultIp != 0x450000 ||
        machine.CPU().LastException().kind != CpuExceptionKind::InvalidOpcode ||
        machine.CPU().LastException().instruction_pointer != 0x450000 ||
        machine.CPU().LastException().vector != CpuExceptionVector::InvalidOpcode) {
        return Fail("CPU invalid-opcode exception dispatch failed") ? 0 : 1;
    }

    // #DE: DIV by zero must report DivideError and preserve the faulting IP.
    if (!bus.Map(0x460000, 0x1000,
                 MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Divide-error page mapping failed") ? 0 : 1;
    }

    const std::uint8_t divideProgram[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00, // MOV EAX,1
        0xB9, 0x00, 0x00, 0x00, 0x00, // MOV ECX,0
        0xBA, 0x00, 0x00, 0x00, 0x00, // MOV EDX,0
        0xF7, 0xF1,                   // DIV ECX
        0xF4
    };

    if (!bus.Write(0x460000, divideProgram, sizeof(divideProgram)) ||
        !bus.Protect(0x460000, 0x1000,
                     MemoryPermission::Read | MemoryPermission::Execute)) {
        return Fail("Divide-error page setup failed") ? 0 : 1;
    }

    observedException = CpuExceptionKind::None;
    observedFault = MemoryFault::None;
    observedFaultIp = 0;
    exceptionObserved = false;

    machine.CPU().SetInstructionPointer(0x460000);
    if (machine.CPU().Run() == 0 ||
        !exceptionObserved ||
        observedException != CpuExceptionKind::DivideError ||
        observedFault != MemoryFault::None ||
        observedFaultIp != 0x46000F ||
        machine.CPU().LastException().kind != CpuExceptionKind::DivideError ||
        machine.CPU().LastException().instruction_pointer != 0x46000F ||
        machine.CPU().LastException().vector != CpuExceptionVector::DivideError) {
        return Fail("CPU divide-error exception dispatch failed") ? 0 : 1;
    }


    // #DE quotient overflow must use the same DivideError exception.
    if (!bus.Map(0x470000, 0x1000,
                 MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Divide-overflow page mapping failed") ? 0 : 1;
    }

    const std::uint8_t divideOverflowProgram[] = {
        0xB8, 0x00, 0x00, 0x00, 0x00, // MOV EAX,0
        0xB9, 0x01, 0x00, 0x00, 0x00, // MOV ECX,1
        0xBA, 0x01, 0x00, 0x00, 0x00, // MOV EDX,1 -> dividend = 2^32
        0xF7, 0xF1,                   // DIV ECX -> quotient = 2^32, overflow
        0xF4
    };

    if (!bus.Write(0x470000, divideOverflowProgram, sizeof(divideOverflowProgram)) ||
        !bus.Protect(0x470000, 0x1000,
                     MemoryPermission::Read | MemoryPermission::Execute)) {
        return Fail("Divide-overflow page setup failed") ? 0 : 1;
    }

    observedException = CpuExceptionKind::None;
    observedFault = MemoryFault::None;
    observedFaultIp = 0;
    exceptionObserved = false;

    machine.CPU().SetInstructionPointer(0x470000);
    if (machine.CPU().Run() == 0 ||
        !exceptionObserved ||
        observedException != CpuExceptionKind::DivideError ||
        observedFault != MemoryFault::None ||
        observedFaultIp != 0x47000F ||
        machine.CPU().LastException().kind != CpuExceptionKind::DivideError ||
        machine.CPU().LastException().instruction_pointer != 0x47000F ||
        machine.CPU().LastException().vector != CpuExceptionVector::DivideError) {
        return Fail("CPU divide-overflow exception dispatch failed") ? 0 : 1;
    }

    if (bus.Map(std::numeric_limits<std::uint64_t>::max() - 0x7FFULL,
                0x1000, MemoryPermission::Read) ||
        bus.LastFault() != MemoryFault::InvalidRange) {
        return Fail("Bus::Map overflow did not report InvalidRange") ? 0 : 1;
    }

    if (bus.MapDevice(std::numeric_limits<std::uint64_t>::max() - 0x7FFULL,
                      0x1000, &ramDevice) ||
        bus.LastFault() != MemoryFault::InvalidRange) {
        return Fail("Bus::MapDevice overflow did not report InvalidRange") ? 0 : 1;
    }

    bus.ClearDevices();
    if (!bus.IsMapped(0x400000, 1)) {
        return Fail("ClearDevices unexpectedly cleared RAM") ? 0 : 1;
    }

    bus.Clear();
    if (bus.IsMapped(0x400000, 1) ||
        bus.Read(0x10000000, deviceRead, 1)) {
        return Fail("Bus Clear did not clear all mappings") ? 0 : 1;
    }

    std::cout << "Machine bus/device architecture test: PASS\n";
    return 0;
}
