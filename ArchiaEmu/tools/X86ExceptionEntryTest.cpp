#include "cpu/x86/ExceptionEntry64.hpp"

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

    Cpu cpu;
    cpu.SetInstructionPointer(0x1111);
    cpu.SetCodeSegment(0x10);
    cpu.SetStackPointer(0x7000);
    cpu.SetStackSegment(0x18);
    cpu.SetRflags(0x202);

    const auto delivered =
        ExceptionEntry64::Deliver(cpu, memory, delivery);
    if (delivered.status != ExceptionEntryStatus::Delivered ||
        delivered.new_rsp != 0x8FD0 ||
        cpu.InstructionPointer() != gate.offset ||
        cpu.CodeSegment() != gate.selector ||
        cpu.Rsp() != 0x8FD0 ||
        cpu.StackSegment() != 0 ||
        cpu.Rflags() != 0x2) {
        return Fail("Atomic exception delivery failed") ? 0 : 1;
    }

    Cpu untouched;
    untouched.SetInstructionPointer(0x1111);
    untouched.SetCodeSegment(0x10);
    untouched.SetStackPointer(0x7000);
    untouched.SetStackSegment(0x18);
    untouched.SetRflags(0x202);

    Memory readOnly;
    if (!readOnly.Map(0x8000, 0x3000, MemoryPermission::Read)) {
        return Fail("Failed to map read-only stack") ? 0 : 1;
    }

    const auto failed =
        ExceptionEntry64::Deliver(untouched, readOnly, delivery);
    if (failed.status != ExceptionEntryStatus::PermissionDenied ||
        untouched.InstructionPointer() != 0x1111 ||
        untouched.CodeSegment() != 0x10 ||
        untouched.Rsp() != 0x7000 ||
        untouched.StackSegment() != 0x18 ||
        untouched.Rflags() != 0x202) {
        return Fail("Failed entry modified CPU state") ? 0 : 1;
    }

    ExceptionDeliveryResult invalid{};
    const auto invalidResult =
        ExceptionEntry64::Deliver(untouched, memory, invalid);
    if (invalidResult.status != ExceptionEntryStatus::InvalidDelivery ||
        untouched.InstructionPointer() != 0x1111) {
        return Fail("Invalid delivery modified CPU state") ? 0 : 1;
    }

    std::cout << "x86-64 atomic exception entry test: PASS\n";
    return 0;
}
