#include "ExceptionStackWriter64.hpp"

#include <array>
#include <limits>

namespace myps5emu::x86 {

ExceptionStackWriteResult ExceptionStackWriter64::Write(
    Memory& memory,
    std::uint64_t stack_top,
    const ExceptionStackFrame64& frame) noexcept
{
    ExceptionStackWriteResult result{};

    const std::size_t byte_count = frame.QwordCount() * 8;
    if (byte_count > stack_top) {
        result.status = ExceptionStackWriteStatus::StackUnderflow;
        return result;
    }

    const std::uint64_t aligned_top = stack_top & ~0xFULL;
    if (byte_count > aligned_top) {
        result.status = ExceptionStackWriteStatus::StackUnderflow;
        return result;
    }

    const std::uint64_t new_rsp =
        aligned_top - static_cast<std::uint64_t>(byte_count);

    if (!memory.IsMapped(new_rsp, byte_count)) {
        result.status = ExceptionStackWriteStatus::Unmapped;
        result.fault = MemoryFault::Unmapped;
        return result;
    }

    if (!memory.HasPermissionAt(
            new_rsp, byte_count, MemoryPermission::Write)) {
        result.status = ExceptionStackWriteStatus::PermissionDenied;
        result.fault = MemoryFault::PermissionDenied;
        return result;
    }

    std::array<std::uint8_t, 48> encoded{};
    if (byte_count > encoded.size() ||
        !frame.Encode(encoded.data(), encoded.size())) {
        result.status = ExceptionStackWriteStatus::MemoryFailure;
        result.fault = MemoryFault::InvalidRange;
        return result;
    }

    if (!memory.Write(new_rsp, encoded.data(), byte_count)) {
        result.status = ExceptionStackWriteStatus::MemoryFailure;
        result.fault = memory.LastFault();
        return result;
    }

    result.status = ExceptionStackWriteStatus::Written;
    result.new_rsp = new_rsp;
    result.fault = MemoryFault::None;
    return result;
}

} // namespace myps5emu::x86
