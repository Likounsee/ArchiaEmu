#include "cpu/Cpu.hpp"
#include "cpu/CpuException.hpp"
#include "cpu/x86/ExceptionDelivery.hpp"
#include "cpu/x86/ExceptionEntry64.hpp"
#include "cpu/x86/ExceptionReturn64.hpp"

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

void WriteQword(
    Memory& memory,
    std::uint64_t address,
    std::uint64_t value)
{
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] =
            static_cast<std::uint8_t>(value >> (i * 8));
    }
    memory.Write(address, bytes.data(), bytes.size());
}

Gdt64 MakeGdt()
{
    Gdt64 gdt;

    GdtCodeSegment64 kernelCode{};
    kernelCode.present = true;
    kernelCode.long_mode = true;
    kernelCode.dpl = 0;
    gdt.SetCodeSegment(5, kernelCode);

    GdtDataSegment64 kernelStack{};
    kernelStack.present = true;
    kernelStack.writable = true;
    kernelStack.dpl = 0;
    gdt.SetDataSegment(2, kernelStack);

    GdtCodeSegment64 userCode{};
    userCode.present = true;
    userCode.long_mode = true;
    userCode.dpl = 3;
    gdt.SetCodeSegment(6, userCode);

    GdtDataSegment64 userStack{};
    userStack.present = true;
    userStack.writable = true;
    userStack.dpl = 3;
    gdt.SetDataSegment(7, userStack);

    return gdt;
}

} // namespace

int main()
{
    Gdt64 gdt = MakeGdt();

    Idt idt;
    IdtGate64 gate{};
    gate.present = true;
    gate.selector = 5U << 3;
    gate.offset = 0xFFFF800000004000ULL;
    gate.type = IdtGateType::Interrupt;
    if (!idt.SetGate(14, gate)) {
        return Fail("Failed to install page-fault interrupt gate") ? 0 : 1;
    }

    Tss64 tss;
    if (!tss.SetRsp0(0x9000)) {
        return Fail("Failed to install kernel exception stack") ? 0 : 1;
    }

    Memory memory;
    if (!memory.Map(
            0x8000,
            0x3000,
            MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Failed to map kernel exception stack") ? 0 : 1;
    }

    Cpu cpu;
    cpu.SetInstructionPointer(0x401234);
    cpu.SetCodeSegment(0x33);
    cpu.SetStackPointer(0x7000);
    cpu.SetStackSegment(0x3B);
    cpu.SetRflags(0x202);

    CpuException exception{};
    exception.kind = CpuExceptionKind::MemoryFault;
    exception.instruction_pointer = cpu.InstructionPointer();
    exception.vector = CpuExceptionVector::PageFault;

    ExceptionDeliveryResolver resolver(idt, gdt, tss);
    const auto delivery = resolver.Resolve(
        exception,
        cpu.CodeSegment(),
        cpu.Rflags(),
        3,
        0xDEAD,
        cpu.Rsp(),
        cpu.StackSegment());

    if (delivery.status != ExceptionDeliveryStatus::Delivered ||
        delivery.target_rip != gate.offset ||
        delivery.target_cs != gate.selector ||
        delivery.target_ss != 0 ||
        delivery.target_rflags != 0x2 ||
        !delivery.stack_frame.has_stack_switch ||
        delivery.stack_frame.rsp != 0x7000 ||
        delivery.stack_frame.ss != 0x3B ||
        !delivery.stack_frame.has_error_code ||
        delivery.stack_frame.error_code != 0xDEAD) {
        return Fail("Page-fault delivery did not build the expected hardware frame")
            ? 0 : 1;
    }

    const auto entered = ExceptionEntry64::Deliver(cpu, memory, delivery);
    if (entered.status != ExceptionEntryStatus::Delivered ||
        cpu.InstructionPointer() != gate.offset ||
        cpu.CodeSegment() != gate.selector ||
        cpu.Rsp() != entered.new_rsp ||
        cpu.StackSegment() != 0 ||
        cpu.Rflags() != 0x2 ||
        entered.new_rsp != 0x8FD0) {
        return Fail("Page-fault exception entry did not update CPU state")
            ? 0 : 1;
    }

    // A page fault pushes an error code below the IRET frame. The handler
    // consumes that code before executing IRETQ.
    cpu.SetStackPointer(cpu.Rsp() + 8);

    const auto returned = ExceptionReturn64::Read(cpu, memory, gdt);
    if (returned.status != ExceptionReturnStatus::Returned ||
        returned.rip != 0x401234 ||
        returned.cs != 0x33 ||
        returned.rflags != 0x202 ||
        returned.rsp != 0x7000 ||
        returned.ss != 0x3B) {
        return Fail("Page-fault handler could not return through IRETQ")
            ? 0 : 1;
    }

    const auto applied = ExceptionReturn64::Apply(cpu, returned);
    if (applied.status != ExceptionReturnStatus::Returned ||
        cpu.InstructionPointer() != 0x401234 ||
        cpu.CodeSegment() != 0x33 ||
        cpu.StackSegment() != 0x3B ||
        cpu.Rsp() != 0x7000 ||
        cpu.Rflags() != 0x202) {
        return Fail("Page-fault exception round-trip did not restore CPU state")
            ? 0 : 1;
    }

    // Trap gates preserve IF while interrupt gates clear it.
    gate.type = IdtGateType::Trap;
    if (!idt.SetGate(14, gate)) {
        return Fail("Failed to install trap gate") ? 0 : 1;
    }

    const auto trapDelivery = resolver.Resolve(
        exception,
        cpu.CodeSegment(),
        cpu.Rflags(),
        3,
        0xBEEF,
        cpu.Rsp(),
        cpu.StackSegment());
    if (trapDelivery.status != ExceptionDeliveryStatus::Delivered ||
        trapDelivery.target_rflags != 0x202) {
        return Fail("Trap gate incorrectly changed IF") ? 0 : 1;
    }

    gate.type = IdtGateType::Interrupt;
    if (!idt.SetGate(14, gate)) {
        return Fail("Failed to restore interrupt gate") ? 0 : 1;
    }

    const auto interruptDelivery = resolver.Resolve(
        exception,
        cpu.CodeSegment(),
        cpu.Rflags(),
        3,
        0xCAFE,
        cpu.Rsp(),
        cpu.StackSegment());
    if (interruptDelivery.status != ExceptionDeliveryStatus::Delivered ||
        interruptDelivery.target_rflags != 0x2) {
        return Fail("Interrupt gate did not clear IF") ? 0 : 1;
    }

    std::cout << "x86-64 exception round-trip test: PASS\n";
    return 0;
}
