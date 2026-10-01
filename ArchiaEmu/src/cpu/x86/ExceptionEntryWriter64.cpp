#include "ExceptionEntryWriter64.hpp"

namespace myps5emu::x86 {

ExceptionEntryWriteResult ExceptionEntryWriter64::Write(
    Memory& memory,
    const ExceptionDeliveryResult& delivery) noexcept
{
    ExceptionEntryWriteResult result{};

    if (delivery.status != ExceptionDeliveryStatus::Delivered) {
        result.status = ExceptionEntryWriteStatus::InvalidDelivery;
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
        result.status = ExceptionEntryWriteStatus::InvalidDelivery;
        return result;
    }

    const auto written =
        ExceptionStackWriter64::Write(memory, stack_top, delivery.stack_frame);

    result.new_rsp = written.new_rsp;
    result.fault = written.fault;

    switch (written.status) {
    case ExceptionStackWriteStatus::Written:
        result.status = ExceptionEntryWriteStatus::Written;
        break;
    case ExceptionStackWriteStatus::StackUnderflow:
        result.status = ExceptionEntryWriteStatus::StackUnderflow;
        break;
    case ExceptionStackWriteStatus::Unmapped:
        result.status = ExceptionEntryWriteStatus::Unmapped;
        break;
    case ExceptionStackWriteStatus::PermissionDenied:
        result.status = ExceptionEntryWriteStatus::PermissionDenied;
        break;
    case ExceptionStackWriteStatus::MemoryFailure:
        result.status = ExceptionEntryWriteStatus::MemoryFailure;
        break;
    }

    return result;
}

} // namespace myps5emu::x86
