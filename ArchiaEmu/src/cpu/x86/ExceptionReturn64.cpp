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

bool ReadQword(
    const Memory& memory,
    std::uint64_t address,
    std::uint64_t& value) noexcept
{
    if (address > std::numeric_limits<std::uint64_t>::max() - 8) {
        return false;
    }

    std::array<std::uint8_t, 8> bytes{};
    if (!memory.Read(address, bytes.data(), bytes.size())) {
        return false;
    }

    value = ReadU64(bytes);
    return true;
}

} // namespace

ExceptionReturnResult ExceptionReturn64::Read(
    const Cpu& cpu,
    const Memory& memory,
    const Gdt64& gdt) noexcept
{
    ExceptionReturnResult result{};

    const std::uint64_t old_rsp = cpu.Rsp();
    if (!ReadQword(memory, old_rsp, result.rip) ||
        !ReadQword(memory, old_rsp + 8, reinterpret_cast<std::uint64_t&>(result.cs)) ||
        !ReadQword(memory, old_rsp + 16, result.rflags)) {
        const auto fault = memory.LastFault();
        result.status = fault == MemoryFault::PermissionDenied
            ? ExceptionReturnStatus::PermissionDenied
            : ExceptionReturnStatus::Unmapped;
        return result;
    }

    if (!IsCanonical48(result.rip)) {
        result.status = ExceptionReturnStatus::InvalidInstructionPointer;
        return result;
    }

    result.cs = static_cast<std::uint16_t>(result.cs);
    GdtCodeSegment64 target{};
    if (!gdt.ResolveCodeSegment(result.cs, target)) {
        result.status = ExceptionReturnStatus::InvalidCodeSegment;
        return result;
    }

    if ((result.cs & 3U) != target.dpl) {
        result.status = ExceptionReturnStatus::InvalidCodeSegment;
        return result;
    }

    if ((result.rflags & 0x2U) == 0) {
        result.status = ExceptionReturnStatus::InvalidRflags;
        return result;
    }

    const std::uint8_t current_cpl =
        static_cast<std::uint8_t>(cpu.CodeSegment() & 3U);
    const std::uint8_t target_cpl =
        static_cast<std::uint8_t>(result.cs & 3U);

    result.rsp = old_rsp + 24;
    result.ss = cpu.StackSegment();

    if (target_cpl != current_cpl) {
        if (!ReadQword(memory, old_rsp + 24, result.rsp) ||
            !ReadQword(memory, old_rsp + 32,
                        reinterpret_cast<std::uint64_t&>(result.ss))) {
            const auto fault = memory.LastFault();
            result.status = fault == MemoryFault::PermissionDenied
                ? ExceptionReturnStatus::PermissionDenied
                : ExceptionReturnStatus::Unmapped;
            return result;
        }

        result.ss = static_cast<std::uint16_t>(result.ss);
        if ((result.ss & 3U) != target_cpl || result.ss == 0 ||
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
