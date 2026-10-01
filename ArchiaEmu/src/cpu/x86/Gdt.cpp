#include "Gdt.hpp"

namespace myps5emu::x86 {

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

    entries_[index] = segment;
    return true;
}

void Gdt64::Clear(std::uint16_t index) noexcept
{
    if (index < kMaxEntries) {
        entries_[index] = {};
    }
}

const GdtCodeSegment64& Gdt64::Entry(std::uint16_t index) const noexcept
{
    static const GdtCodeSegment64 invalid{};
    return index < kMaxEntries ? entries_[index] : invalid;
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

    // TI=1 selects the LDT; this minimal x86-64 model only implements GDT.
    if ((selector & 0x4U) != 0U || index >= kMaxEntries) {
        return false;
    }

    const auto& entry = entries_[index];
    if (!entry.IsValidLongModeTarget()) {
        return false;
    }

    segment = entry;
    return true;
}

} // namespace myps5emu::x86
