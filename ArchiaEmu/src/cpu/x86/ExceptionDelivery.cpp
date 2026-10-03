#include "ExceptionDelivery.hpp"

#include "ExceptionDispatcher.hpp"

namespace myps5emu::x86 {

ExceptionDeliveryResolver::ExceptionDeliveryResolver(
    const Idt& idt,
    const Gdt64& gdt,
    const Tss64& tss) noexcept
    : idt_(idt), gdt_(gdt), tss_(tss)
{
}

ExceptionDeliveryResult ExceptionDeliveryResolver::Resolve(
    const CpuException& exception,
    std::uint16_t current_cs,
    std::uint64_t current_rflags,
    std::uint8_t current_cpl,
    std::uint64_t error_code,
    std::uint64_t current_rsp,
    std::uint64_t current_ss) const noexcept
{
    ExceptionDeliveryResult result{};

    ExceptionDispatcher dispatcher(idt_);
    const auto dispatch = dispatcher.Resolve(exception);

    switch (dispatch.status) {
    case IdtDispatchStatus::NoVector:
        result.status = ExceptionDeliveryStatus::NoVector;
        return result;
    case IdtDispatchStatus::NotPresent:
        result.status = ExceptionDeliveryStatus::NotPresent;
        return result;
    case IdtDispatchStatus::InvalidGate:
        result.status = ExceptionDeliveryStatus::InvalidGate;
        return result;
    case IdtDispatchStatus::Delivered:
        break;
    }

    ExceptionTargetResolver targetResolver(gdt_);
    const auto target =
        targetResolver.Resolve(dispatch.gate, current_cpl);

    if (target.status != ExceptionTargetStatus::Valid) {
        result.status = ExceptionDeliveryStatus::InvalidTarget;
        return result;
    }

    ExceptionStackResolver stackResolver(tss_);
    const auto stack = stackResolver.Resolve(
        dispatch.gate.ist, current_cpl, target.segment.dpl);
    if (stack.status == ExceptionStackStatus::InvalidIst ||
        stack.status == ExceptionStackStatus::InvalidPrivilegeLevel) {
        result.status = ExceptionDeliveryStatus::InvalidStack;
        return result;
    }
    if (stack.status == ExceptionStackStatus::Unavailable) {
        result.status = ExceptionDeliveryStatus::StackUnavailable;
        return result;
    }

    result.stack = stack;
    result.stack_frame.rip = exception.instruction_pointer;
    result.stack_frame.cs = current_cs;
    result.stack_frame.rflags = current_rflags;
    result.stack_frame.has_error_code =
        ExceptionFrame64::HasHardwareErrorCode(exception.vector);
    result.stack_frame.error_code = error_code;
    // SS:RSP are pushed only when the interrupt changes CPL or uses
    // an IST stack. A same-CPL interrupt without IST has the three-qword
    // RIP/CS/RFLAGS frame (plus an optional error code).
    result.stack_frame.has_stack_switch =
        target.segment.dpl < current_cpl || dispatch.gate.ist != 0;
    result.stack_frame.rsp = current_rsp;
    result.stack_frame.ss = current_ss;
    result.frame = ExceptionFrame64::Build(
        exception, current_cs, current_rflags, error_code);
    result.target_rip = dispatch.gate.offset;
    result.target_cs = dispatch.gate.selector;
    // In 64-bit mode a CPL-changing or IST stack switch forces SS to NULL
    // with the target CPL in its RPL field. Otherwise SS is preserved.
    if (target.segment.dpl < current_cpl || dispatch.gate.ist != 0) {
        result.target_ss = target.segment.dpl;
    } else {
        result.target_ss = current_ss;
    }
    constexpr std::uint64_t kRflagsInterruptEnable = 1ULL << 9;
    constexpr std::uint64_t kRflagsTrap = 1ULL << 8;
    constexpr std::uint64_t kRflagsNestedTask = 1ULL << 14;
    constexpr std::uint64_t kRflagsResume = 1ULL << 16;
    constexpr std::uint64_t kRflagsVirtual8086 = 1ULL << 17;
    result.target_rflags = current_rflags &
        ~(kRflagsTrap | kRflagsNestedTask | kRflagsResume | kRflagsVirtual8086);
    if (dispatch.gate.type == IdtGateType::Interrupt) {
        result.target_rflags &= ~kRflagsInterruptEnable;
    }
    result.target_segment = target.segment;
    result.status = ExceptionDeliveryStatus::Delivered;
    return result;
}

} // namespace myps5emu::x86
