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
    result.frame = ExceptionFrame64::Build(
        exception, current_cs, current_rflags, error_code);
    result.target_rip = dispatch.gate.offset;
    result.target_cs = dispatch.gate.selector;
    result.target_segment = target.segment;
    result.status = ExceptionDeliveryStatus::Delivered;
    return result;
}

} // namespace myps5emu::x86
