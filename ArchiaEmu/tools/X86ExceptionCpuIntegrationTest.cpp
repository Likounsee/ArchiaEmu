#include "cpu/Cpu.hpp"
#include "cpu/x86/Idt.hpp"
#include "cpu/x86/Gdt.hpp"
#include "cpu/x86/Tss64.hpp"
#include "memory/Memory.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {
bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

bool Read64(
    const Memory& memory,
    std::uint64_t address,
    std::uint64_t& value)
{
    std::array<std::uint8_t, 8> bytes{};
    if (!memory.Read(address, bytes.data(), bytes.size())) {
        return false;
    }

    value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
    }
    return true;
}
}

int main()
{
    Memory memory;
    if (!memory.Map(
            0x8000,
            0x2000,
            MemoryPermission::Read | MemoryPermission::Write)) {
        return 1;
    }

    Idt idt;
    Gdt64 gdt;
    Tss64 tss;

    GdtCodeSegment64 kernel_code{};
    kernel_code.present = true;
    kernel_code.long_mode = true;
    kernel_code.dpl = 0;
    if (!gdt.SetCodeSegment(5, kernel_code)) {
        return Fail("failed to install kernel code segment") ? 0 : 1;
    }

    IdtGate64 gate{};
    gate.present = true;
    gate.selector = static_cast<std::uint16_t>(5U << 3);
    gate.offset = 0xFFFF800000001000ULL;
    if (!idt.SetGate(14, gate)) {
        return Fail("failed to install page-fault gate") ? 0 : 1;
    }

    tss.SetRsp0(0xA000);

    Cpu cpu;
    cpu.ConnectMemory(&memory);
    cpu.SetExceptionArchitecture(&idt, &gdt, &tss);
    cpu.SetCodeSegment(0x1B);
    cpu.SetStackSegment(0x23);
    cpu.SetStackPointer(0x9000);
    cpu.SetInstructionPointer(0x4000);
    cpu.SetRflags(0x246);

    bool callback_called = false;
    cpu.SetExceptionHandler([&](Cpu&, const CpuException&) {
        callback_called = true;
        return true;
    });

    CpuException exception{};
    exception.kind = CpuExceptionKind::MemoryFault;
    exception.vector = CpuExceptionVector::PageFault;
    exception.instruction_pointer = 0x4000;
    exception.page_fault_error = 0x5;

    if (!cpu.DeliverException(exception)) {
        return Fail("hardware exception delivery was not accepted") ? 0 : 1;
    }

    // A software INT from CPL3 must honor the selected IDT gate DPL.
    // The gate below intentionally has DPL=0; no GP gate is installed, so
    // correct behavior is a failed nested #GP delivery with no state change.
    IdtGate64 softwareGate{};
    softwareGate.present = true;
    softwareGate.selector = static_cast<std::uint16_t>(5U << 3);
    softwareGate.offset = 0xFFFF800000002000ULL;
    softwareGate.dpl = 0;
    if (!idt.SetGate(3, softwareGate)) {
        return Fail("failed to install software interrupt gate") ? 0 : 1;
    }

    Cpu softwareCpu;
    softwareCpu.ConnectMemory(&memory);
    softwareCpu.SetExceptionArchitecture(&idt, &gdt, &tss);
    softwareCpu.SetCodeSegment(0x1B);
    softwareCpu.SetStackSegment(0x23);
    softwareCpu.SetStackPointer(0x9000);
    softwareCpu.SetInstructionPointer(0x5000);
    softwareCpu.SetRflags(0x246);

    if (softwareCpu.DeliverException({
            CpuExceptionKind::SoftwareInterrupt,
            0x5000,
            MemoryFault::None,
            CpuExceptionVector::Breakpoint}) ||
        softwareCpu.InstructionPointer() != 0x5000 ||
        softwareCpu.CodeSegment() != 0x1B ||
        softwareCpu.Rsp() != 0x9000) {
        return Fail("software interrupt bypassed IDT DPL") ? 0 : 1;
    }

    if (callback_called ||
        cpu.CodeSegment() != 0x28 ||
        cpu.StackSegment() != 0 ||
        cpu.InstructionPointer() != gate.offset ||
        cpu.Rsp() != 0x9FD0 ||
        cpu.Rflags() != 0x46) {
        return Fail("CPU state was not transferred through the hardware exception path")
            ? 0 : 1;
    }

    std::uint64_t value = 0;
    const std::array<std::uint64_t, 6> expected = {
        0x5, 0x4000, 0x1B, 0x246, 0x9000, 0x23
    };

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (!Read64(memory, 0x9FD0 + i * 8, value) ||
            value != expected[i]) {
            return Fail("hardware exception frame was not written correctly")
                ? 0 : 1;
        }
    }

    std::cout << "x86 CPU hardware exception integration test: PASS\n";
    return 0;
}
