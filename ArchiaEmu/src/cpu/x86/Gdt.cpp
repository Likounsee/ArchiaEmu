#include "Gdt.hpp"

namespace myps5emu::x86 {

bool GdtDataSegment64::IsValidLongModeStackSegment() const noexcept
{
    return present && writable && dpl <= 3;
}

bool GdtCodeSegment64::IsValidLongModeTarget() const noexcept
{
    return present && !conforming && long_mode &&
           !default_operand_size_32 && dpl <= 3;
}

Gdt64::Gdt64() noexcept = default;

bool Gdt64::SetCodeSegment(
    std::uint16_t index,
    const GdtCodeSegment64& segment) noexcept
{
    if (index >= kMaxEntries || !segment.IsValidLongModeTarget()) {
        return false;
    }

    code_entries_[index] = segment;
    return true;
}

bool Gdt64::SetDataSegment(
    std::uint16_t index,
    const GdtDataSegment64& segment) noexcept
{
    if (index >= kMaxEntries || !segment.IsValidLongModeStackSegment()) {
        return false;
    }

    data_entries_[index] = segment;
    return true;
}

void Gdt64::Clear(std::uint16_t index) noexcept
{
    if (index < kMaxEntries) {
        code_entries_[index] = {};
        data_entries_[index] = {};
    }
}

const GdtCodeSegment64& Gdt64::Entry(std::uint16_t index) const noexcept
{
    static const GdtCodeSegment64 invalid{};
    return index < kMaxEntries ? code_entries_[index] : invalid;
}

std::uint16_t Gdt64::Limit() const noexcept
{
    return kMaxLimit;
}

bool Gdt64::ResolveCodeSegment(
    std::uint16_t selector,
    GdtCodeSegment64& segment) const noexcept
{
    const std::uint16_t index = static_cast<std::uint16_t>(selector >> 3);

    if ((selector & 0x4U) != 0U || index >= kMaxEntries) {
        return false;
    }

    const auto& entry = code_entries_[index];
    if (!entry.IsValidLongModeTarget()) {
        return false;
    }

    segment = entry;
    return true;
}

bool Gdt64::ResolveDataSegment(
    std::uint16_t selector,
    GdtDataSegment64& segment) const noexcept
{
    const std::uint16_t index = static_cast<std::uint16_t>(selector >> 3);

    if ((selector & 0x4U) != 0U || index >= kMaxEntries) {
        return false;
    }

    const auto& entry = data_entries_[index];
    if (!entry.IsValidLongModeStackSegment() ||
        (selector & 0x3U) != entry.dpl) {
        return false;
    }

    segment = entry;
    return true;
}

} // namespace myps5emu::x86
