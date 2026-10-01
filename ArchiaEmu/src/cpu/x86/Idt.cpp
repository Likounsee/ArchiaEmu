#include "Idt.hpp"

namespace myps5emu::x86 {

namespace {

std::uint8_t TypeAttribute(const IdtGate64& gate) noexcept
{
    return static_cast<std::uint8_t>(
        (gate.dpl & 0x03U) << 5U) |
        (gate.present ? 0x80U : 0x00U) |
        static_cast<std::uint8_t>(gate.type);
}

} // namespace

bool IdtGate64::IsValid() const noexcept
{
    const auto type = static_cast<std::uint8_t>(type);
    return (type == static_cast<std::uint8_t>(IdtGateType::Interrupt) ||
            type == static_cast<std::uint8_t>(IdtGateType::Trap)) &&
           dpl <= 3 &&
           ist <= 7;
}

std::array<std::uint8_t, 16> IdtGate64::Encode() const noexcept
{
    std::array<std::uint8_t, 16> bytes{};

    const std::uint16_t offset_low =
        static_cast<std::uint16_t>(offset & 0xFFFFU);
    const std::uint16_t offset_middle =
        static_cast<std::uint16_t>((offset >> 16U) & 0xFFFFU);
    const std::uint32_t offset_high =
        static_cast<std::uint32_t>(offset >> 32U);

    bytes[0] = static_cast<std::uint8_t>(offset_low);
    bytes[1] = static_cast<std::uint8_t>(offset_low >> 8U);
    bytes[2] = static_cast<std::uint8_t>(selector);
    bytes[3] = static_cast<std::uint8_t>(selector >> 8U);
    bytes[4] = static_cast<std::uint8_t>(ist & 0x07U);
    bytes[5] = TypeAttribute(*this);
    bytes[6] = static_cast<std::uint8_t>(offset_middle);
    bytes[7] = static_cast<std::uint8_t>(offset_middle >> 8U);
    bytes[8] = static_cast<std::uint8_t>(offset_high);
    bytes[9] = static_cast<std::uint8_t>(offset_high >> 8U);
    bytes[10] = static_cast<std::uint8_t>(offset_high >> 16U);
    bytes[11] = static_cast<std::uint8_t>(offset_high >> 24U);

    return bytes;
}

IdtGate64 IdtGate64::Decode(
    const std::array<std::uint8_t, 16>& bytes) noexcept
{
    IdtGate64 gate{};

    const std::uint16_t offset_low =
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8U);

    const std::uint16_t offset_middle =
        static_cast<std::uint16_t>(bytes[6]) |
        (static_cast<std::uint16_t>(bytes[7]) << 8U);

    const std::uint32_t offset_high =
        static_cast<std::uint32_t>(bytes[8]) |
        (static_cast<std::uint32_t>(bytes[9]) << 8U) |
        (static_cast<std::uint32_t>(bytes[10]) << 16U) |
        (static_cast<std::uint32_t>(bytes[11]) << 24U);

    gate.offset =
        static_cast<std::uint64_t>(offset_low) |
        (static_cast<std::uint64_t>(offset_middle) << 16U) |
        (static_cast<std::uint64_t>(offset_high) << 32U);

    gate.selector =
        static_cast<std::uint16_t>(bytes[2]) |
        (static_cast<std::uint16_t>(bytes[3]) << 8U);

    gate.ist = static_cast<std::uint8_t>(bytes[4] & 0x07U);

    const std::uint8_t attributes = bytes[5];
    gate.present = (attributes & 0x80U) != 0;
    gate.dpl = static_cast<std::uint8_t>((attributes >> 5U) & 0x03U);

    const std::uint8_t type = attributes & 0x0FU;
    if (type == static_cast<std::uint8_t>(IdtGateType::Trap)) {
        gate.type = IdtGateType::Trap;
    } else {
        gate.type = IdtGateType::Interrupt;
    }

    return gate;
}

Idt::Idt() noexcept = default;

bool Idt::SetGate(
    std::uint8_t vector,
    const IdtGate64& gate) noexcept
{
    if (!gate.IsValid()) {
        return false;
    }

    entries_[vector] = gate;
    return true;
}

void Idt::ClearGate(std::uint8_t vector) noexcept
{
    entries_[vector] = {};
}

const IdtGate64& Idt::Gate(std::uint8_t vector) const noexcept
{
    return entries_[vector];
}

bool Idt::IsPresent(std::uint8_t vector) const noexcept
{
    return entries_[vector].present;
}

std::uint16_t Idt::Limit() const noexcept
{
    return kLimit;
}

} // namespace myps5emu::x86
