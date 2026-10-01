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
    std::uint64_t error_code) const noexcept
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
    result.stack_frame.privilege_stack_switch =
        target.segment.dpl < current_cpl;
    if (result.stack_frame.privilege_stack_switch) {
        result.stack_frame.rsp = stack.stack_pointer;
    }
    result.frame = ExceptionFrame64::Build(
        exception, current_cs, current_rflags, error_code);
    result.target_rip = dispatch.gate.offset;
    result.target_cs = dispatch.gate.selector;
    constexpr std::uint64_t kRflagsInterruptEnable = 1ULL << 9;
    result.target_rflags = current_rflags;
    if (dispatch.gate.type == IdtGateType::Interrupt) {
        result.target_rflags &= ~kRflagsInterruptEnable;
    }
    result.target_segment = target.segment;
    result.status = ExceptionDeliveryStatus::Delivered;
    return result;
}

} // namespace myps5emu::x86
