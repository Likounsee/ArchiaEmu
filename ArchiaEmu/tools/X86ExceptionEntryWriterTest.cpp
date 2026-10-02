#include "cpu/CpuException.hpp"
#include "cpu/x86/ExceptionEntryWriter64.hpp"

#include <array>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    Idt idt;
    Gdt64 gdt;
    GdtCodeSegment64 code{};
    code.present = true;
    code.long_mode = true;
    code.dpl = 0;
    if (!gdt.SetCodeSegment(5, code)) {
        return Fail("Failed to install target code segment") ? 0 : 1;
    }

    IdtGate64 gate{};
    gate.present = true;
    gate.selector = 5U << 3;
    gate.offset = 0xFFFF800000004000ULL;
    if (!idt.SetGate(14, gate)) {
        return Fail("Failed to install page-fault gate") ? 0 : 1;
    }

    Tss64 tss;
    tss.SetRsp0(0x9000);

    CpuException exception{};
    exception.kind = CpuExceptionKind::MemoryFault;
    exception.instruction_pointer = 0x123456789ABCDEF0ULL;
    exception.vector = CpuExceptionVector::PageFault;

    ExceptionDeliveryResolver resolver(idt, gdt, tss);
    const auto delivery = resolver.Resolve(
        exception, 0x10, 0x202, 3, 0x5, 0x7000, 0x18);

    Memory memory;
    if (!memory.Map(0x8000, 0x3000,
                    MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Failed to map exception stack") ? 0 : 1;
    }

    const auto written = ExceptionEntryWriter64::Write(memory, delivery);
    if (written.status != ExceptionEntryWriteStatus::Written ||
        written.new_rsp != 0x8FD0 ||
        written.fault != MemoryFault::None) {
        return Fail("Privilege-transition exception entry write failed")
            ? 0 : 1;
    }

    std::array<std::uint8_t, 48> bytes{};
    if (!memory.Read(written.new_rsp, bytes.data(), bytes.size()) ||
        bytes[0] != 0x05 ||
        bytes[8] != 0xF0 ||
        bytes[9] != 0xDE) {
        return Fail("Written exception entry frame has wrong contents")
            ? 0 : 1;
    }

    gate.ist = 0;
    idt.SetGate(14, gate);
    const auto sameCpl = resolver.Resolve(
        exception, 0x10, 0x202, 0, 0x5, 0xA000, 0x20);
    const auto sameCplWrite =
        ExceptionEntryWriter64::Write(memory, sameCpl);
    if (sameCplWrite.status != ExceptionEntryWriteStatus::Written ||
        sameCplWrite.new_rsp != 0x9FE0) {
        return Fail("Same-CPL exception entry write failed") ? 0 : 1;
    }

    std::array<std::uint8_t, 32> sameBytes{};
    if (!memory.Read(sameCplWrite.new_rsp, sameBytes.data(), sameBytes.size()) ||
        sameBytes[0] != 0x20 || sameBytes[1] != 0x00 ||
        sameBytes[8] != 0xF0 || sameBytes[9] != 0xDE ||
        sameBytes[16] != 0x10 || sameBytes[24] != 0x02 ||
        sameBytes[25] != 0x02) {
        std::cerr << "same-CPL bytes: "
                  << std::hex
                  << static_cast<unsigned>(sameBytes[0]) << " "
                  << static_cast<unsigned>(sameBytes[8]) << " "
                  << static_cast<unsigned>(sameBytes[16]) << " "
                  << static_cast<unsigned>(sameBytes[24])
                  << std::dec << '\\n';
        return Fail("Same-CPL exception frame layout is wrong") ? 0 : 1;
    }

    if (!memory.Protect(0x8000, 0x3000, MemoryPermission::Read)) {
        return Fail("Failed to protect exception stack") ? 0 : 1;
    }
    const auto denied =
        ExceptionEntryWriter64::Write(memory, delivery);
    if (denied.status != ExceptionEntryWriteStatus::PermissionDenied ||
        denied.fault != MemoryFault::PermissionDenied) {
        return Fail("Read-only exception stack was not rejected")
            ? 0 : 1;
    }

    ExceptionDeliveryResult invalid{};
    const auto invalidWrite = ExceptionEntryWriter64::Write(memory, invalid);
    if (invalidWrite.status != ExceptionEntryWriteStatus::InvalidDelivery) {
        return Fail("Invalid delivery was accepted") ? 0 : 1;
    }

    std::cout << "x86-64 exception entry writer test: PASS\n";
    return 0;
}
