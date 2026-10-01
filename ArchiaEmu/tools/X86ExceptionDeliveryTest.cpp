#include "cpu/CpuException.hpp"
#include "cpu/x86/ExceptionDelivery.hpp"

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
    if (!gate.IsValid() || !idt.SetGate(14, gate)) {
        return Fail("Failed to install page-fault IDT gate") ? 0 : 1;
    }

    CpuException exception{};
    exception.kind = CpuExceptionKind::MemoryFault;
    exception.instruction_pointer = 0x123456789ABCDEF0ULL;
    exception.vector = CpuExceptionVector::PageFault;

    Tss64 tss;
    tss.SetRsp0(0x9000);

    ExceptionDeliveryResolver resolver(idt, gdt, tss);
    const auto delivered = resolver.Resolve(
        exception, 0x10, 0x202, 3, 0x5, 0x7000, 0x18);

    if (delivered.status != ExceptionDeliveryStatus::Delivered ||
        delivered.frame.rip != exception.instruction_pointer ||
        delivered.frame.cs != 0x10 ||
        delivered.frame.rflags != 0x202 ||
        !delivered.frame.has_error_code ||
        delivered.frame.error_code != 0x5 ||
        delivered.target_rip != gate.offset ||
        delivered.target_cs != gate.selector ||
        delivered.target_rflags != 0x2 ||
        delivered.stack.status != ExceptionStackStatus::StackSelected ||
        delivered.stack.stack_pointer != 0x9000 ||
        !delivered.stack_frame.has_saved_stack ||
        delivered.stack_frame.rsp != 0x7000 ||
        delivered.stack_frame.ss != 0x18) {
        return Fail("Valid exception delivery resolution failed") ? 0 : 1;
    }

    gate.type = IdtGateType::Trap;
    gate.ist = 2;
    tss.SetIst(2, 0xA000);
    idt.SetGate(14, gate);
    const auto istDelivered = resolver.Resolve(
        exception, 0x10, 0x202, 0, 0x5);
    if (istDelivered.status != ExceptionDeliveryStatus::Delivered ||
        istDelivered.target_rflags != 0x202 ||
        istDelivered.stack.status != ExceptionStackStatus::StackSelected ||
        istDelivered.stack.stack_pointer != 0xA000) {
        return Fail("IST stack selection during delivery failed") ? 0 : 1;
    }

    gate.ist = 7;
    tss.SetIst(7, 0);
    idt.SetGate(14, gate);
    if (resolver.Resolve(exception, 0x10, 0x202, 0, 0x5).status !=
        ExceptionDeliveryStatus::StackUnavailable) {
        return Fail("Unavailable IST stack was not propagated") ? 0 : 1;
    }

    gate.ist = 0;
    idt.SetGate(14, gate);

    const sameCpl = resolver.Resolve(
        exception, 0x10, 0x202, 0, 0x5, 0xB000, 0x20);
    if (sameCpl.status != ExceptionDeliveryStatus::Delivered ||
        sameCpl.stack.status != ExceptionStackStatus::NoStackSwitch ||
        !sameCpl.stack_frame.has_saved_stack ||
        sameCpl.stack_frame.rsp != 0xB000 ||
        sameCpl.stack_frame.ss != 0x20) {
        return Fail("Same-CPL exception frame did not preserve SS:RSP")
            ? 0 : 1;
    }

    idt.ClearGate(14);
    if (resolver.Resolve(exception, 0x10, 0x202, 0, 0x5).status !=
        ExceptionDeliveryStatus::NotPresent) {
        return Fail("Missing IDT gate was not rejected") ? 0 : 1;
    }

    gate.selector = 0;
    idt.SetGate(14, gate);
    if (resolver.Resolve(exception, 0x10, 0x202, 0, 0x5).status !=
        ExceptionDeliveryStatus::InvalidTarget) {
        return Fail("Invalid target selector was not rejected") ? 0 : 1;
    }

    std::cout << "x86-64 exception delivery resolution test: PASS\n";
    return 0;
}
