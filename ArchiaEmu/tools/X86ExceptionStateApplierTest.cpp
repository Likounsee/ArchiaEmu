#include "cpu/x86/ExceptionStateApplier64.hpp"

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

    Cpu cpu;
    cpu.SetInstructionPointer(0x1111);
    cpu.SetCodeSegment(0x10);
    cpu.SetStackPointer(0x7000);
    cpu.SetStackSegment(0x18);
    cpu.SetRflags(0x202);

    const auto applied =
        ExceptionStateApplier64::Apply(cpu, delivery, 0x8FD0);
    if (applied.status != ExceptionStateApplyStatus::Applied ||
        cpu.InstructionPointer() != gate.offset ||
        cpu.CodeSegment() != gate.selector ||
        cpu.Rsp() != 0x8FD0 ||
        cpu.StackSegment() != 0 ||
        cpu.Rflags() != 0x2) {
        return Fail("Privilege-transition CPU state application failed")
            ? 0 : 1;
    }

    gate.ist = 0;
    idt.SetGate(14, gate);
    const auto sameCpl = resolver.Resolve(
        exception, 0x10, 0x202, 0, 0x5, 0xA000, 0x20);
    const auto sameApplied =
        ExceptionStateApplier64::Apply(cpu, sameCpl, 0x9FD0);
    if (sameApplied.status != ExceptionStateApplyStatus::Applied ||
        cpu.StackSegment() != 0x20 ||
        cpu.Rsp() != 0x9FD0 ||
        cpu.Rflags() != 0x2) {
        return Fail("Same-CPL CPU state application failed") ? 0 : 1;
    }

    ExceptionDeliveryResult invalid{};
    const auto rejected =
        ExceptionStateApplier64::Apply(cpu, invalid, 0x1000);
    if (rejected.status != ExceptionStateApplyStatus::InvalidDelivery) {
        return Fail("Invalid delivery changed CPU state") ? 0 : 1;
    }

    std::cout << "x86-64 exception state applier test: PASS\n";
    return 0;
}
