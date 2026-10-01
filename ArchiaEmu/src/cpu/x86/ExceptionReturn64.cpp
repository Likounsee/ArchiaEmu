#include "ExceptionReturn64.hpp"

#include <array>
#include <limits>

namespace myps5emu::x86 {

namespace {

std::uint64_t ReadU64(const std::array<std::uint8_t, 8>& bytes) noexcept
{
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
    }
    return value;
}

bool IsCanonical48(std::uint64_t value) noexcept
{
    const std::uint64_t upper = value >> 48;
    return upper == 0 || upper == 0xFFFF;
}

bool AddOffset(
    std::uint64_t address,
    std::uint64_t offset,
    std::uint64_t& result) noexcept
{
    if (address > std::numeric_limits<std::uint64_t>::max() - offset) {
        return false;
    }
    result = address + offset;
    return true;
}

bool ReadQword(
    const Memory& memory,
    std::uint64_t address,
    std::uint64_t& value) noexcept
{
    std::array<std::uint8_t, 8> bytes{};
    if (!memory.Read(address, bytes.data(), bytes.size())) {
        return false;
    }

    value = ReadU64(bytes);
    return true;
}

ExceptionReturnStatus ReadFailureStatus(const Memory& memory) noexcept
{
    return memory.LastFault() == MemoryFault::PermissionDenied
        ? ExceptionReturnStatus::PermissionDenied
        : ExceptionReturnStatus::Unmapped;
}

} // namespace

ExceptionReturnResult ExceptionReturn64::Read(
    const Cpu& cpu,
    const Memory& memory,
    const Gdt64& gdt) noexcept
{
    ExceptionReturnResult result{};
    const std::uint64_t old_rsp = cpu.Rsp();

    std::uint64_t address = 0;
    if (!ReadQword(memory, old_rsp, result.rip)) {
        result.status = ReadFailureStatus(memory);
        return result;
    }

    if (!AddOffset(old_rsp, 8, address) ||
        !ReadQword(memory, address, address)) {
        result.status = ReadFailureStatus(memory);
        return result;
    }
    result.cs = static_cast<std::uint16_t>(address);

    if (!AddOffset(old_rsp, 16, address) ||
        !ReadQword(memory, address, result.rflags)) {
        result.status = ReadFailureStatus(memory);
        return result;
    }

    if (!IsCanonical48(result.rip)) {
        result.status = ExceptionReturnStatus::InvalidInstructionPointer;
        return result;
    }

    GdtCodeSegment64 target{};
    if (!gdt.ResolveCodeSegment(result.cs, target)) {
        result.status = ExceptionReturnStatus::InvalidCodeSegment;
        return result;
    }

    if ((result.cs & 3U) != target.dpl) {
        result.status = ExceptionReturnStatus::InvalidCodeSegment;
        return result;
    }

    if ((result.rflags & 0x2U) == 0 ||
        (result.rflags & ((1ULL << 3) | (1ULL << 5) |
                          (1ULL << 15) | (1ULL << 22))) != 0) {
        result.status = ExceptionReturnStatus::InvalidRflags;
        return result;
    }

    const std::uint8_t current_cpl =
        static_cast<std::uint8_t>(cpu.CodeSegment() & 3U);
    const std::uint8_t target_cpl =
        static_cast<std::uint8_t>(result.cs & 3U);

    // IRET may return to the same CPL or to a less privileged level
    // (numerically greater CPL), but never to a more privileged level.
    if (target_cpl < current_cpl) {
        result.status = ExceptionReturnStatus::InvalidCodeSegment;
        return result;
    }

    if (!AddOffset(old_rsp, 24, result.rsp)) {
        result.status = ExceptionReturnStatus::InvalidStack;
        return result;
    }
    result.ss = cpu.StackSegment();

    if (target_cpl != current_cpl) {
        if (!AddOffset(old_rsp, 24, address) ||
            !ReadQword(memory, address, result.rsp) ||
            !AddOffset(old_rsp, 32, address) ||
            !ReadQword(memory, address, address)) {
            result.status = ReadFailureStatus(memory);
            return result;
        }

        result.ss = static_cast<std::uint16_t>(address);
        GdtDataSegment64 stack_segment{};
        if (!gdt.ResolveDataSegment(result.ss, stack_segment) ||
            stack_segment.dpl != target_cpl ||
            (result.ss & 3U) != target_cpl || result.ss == 0 ||
            result.rsp == 0 || !IsCanonical48(result.rsp)) {
            result.status = ExceptionReturnStatus::InvalidStack;
            return result;
        }
    }

    result.status = ExceptionReturnStatus::Returned;
    return result;
}

ExceptionReturnResult ExceptionReturn64::Apply(
    Cpu& cpu,
    const ExceptionReturnResult& result) noexcept
{
    if (result.status != ExceptionReturnStatus::Returned) {
        return {ExceptionReturnStatus::InvalidFrame};
    }

    cpu.SetInstructionPointer(result.rip);
    cpu.SetCodeSegment(result.cs);
    cpu.SetRflags(result.rflags);
    cpu.SetStackPointer(result.rsp);
    cpu.SetStackSegment(result.ss);
    return result;
}

} // namespace myps5emu::x86
