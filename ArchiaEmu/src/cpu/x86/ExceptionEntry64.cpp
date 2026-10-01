#include "ExceptionEntry64.hpp"

namespace myps5emu::x86 {

ExceptionEntryResult ExceptionEntry64::Deliver(
    Cpu& cpu,
    Memory& memory,
    const ExceptionDeliveryResult& delivery) noexcept
{
    ExceptionEntryResult result{};

    if (delivery.status != ExceptionDeliveryStatus::Delivered) {
        result.status = ExceptionEntryStatus::InvalidDelivery;
        return result;
    }

    std::uint64_t stack_top = 0;
    switch (delivery.stack.status) {
    case ExceptionStackStatus::StackSelected:
        stack_top = delivery.stack.stack_pointer;
        break;
    case ExceptionStackStatus::NoStackSwitch:
        stack_top = delivery.stack_frame.rsp;
        break;
    case ExceptionStackStatus::InvalidIst:
    case ExceptionStackStatus::Unavailable:
    case ExceptionStackStatus::InvalidPrivilegeLevel:
        result.status = ExceptionEntryStatus::InvalidDelivery;
        return result;
    }

    const auto written =
        ExceptionStackWriter64::Write(memory, stack_top, delivery.stack_frame);

    if (written.status != ExceptionStackWriteStatus::Written) {
        result.new_rsp = 0;
        result.fault = written.fault;
        switch (written.status) {
        case ExceptionStackWriteStatus::StackUnderflow:
            result.status = ExceptionEntryStatus::StackUnderflow;
            break;
        case ExceptionStackWriteStatus::Unmapped:
            result.status = ExceptionEntryStatus::Unmapped;
            break;
        case ExceptionStackWriteStatus::PermissionDenied:
            result.status = ExceptionEntryStatus::PermissionDenied;
            break;
        case ExceptionStackWriteStatus::MemoryFailure:
            result.status = ExceptionEntryStatus::MemoryFailure;
            break;
        case ExceptionStackWriteStatus::Written:
            break;
        }
        return result;
    }

    const auto applied =
        ExceptionStateApplier64::Apply(cpu, delivery, written.new_rsp);
    if (applied.status != ExceptionStateApplyStatus::Applied) {
        result.status = ExceptionEntryStatus::InvalidDelivery;
        return result;
    }

    result.status = ExceptionEntryStatus::Delivered;
    result.new_rsp = written.new_rsp;
    result.fault = MemoryFault::None;
    return result;
}

} // namespace myps5emu::x86
