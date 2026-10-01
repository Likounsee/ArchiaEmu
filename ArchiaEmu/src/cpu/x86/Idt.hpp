#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace myps5emu::x86 {

enum class IdtGateType : std::uint8_t {
    Interrupt = 0x0E,
    Trap = 0x0F
};

struct IdtGate64 {
    std::uint64_t offset = 0;
    std::uint16_t selector = 0;
    std::uint8_t ist = 0;
    IdtGateType type = IdtGateType::Interrupt;
    std::uint8_t dpl = 0;
    bool present = false;

    bool IsValid() const noexcept;
    std::array<std::uint8_t, 16> Encode() const noexcept;
    static IdtGate64 Decode(const std::array<std::uint8_t, 16>& bytes) noexcept;
};

class Idt {
public:
    static constexpr std::size_t kEntryCount = 256;
    static constexpr std::uint16_t kLimit =
        static_cast<std::uint16_t>(kEntryCount * 16 - 1);

    Idt() noexcept;

    bool SetGate(std::uint8_t vector, const IdtGate64& gate) noexcept;
    void ClearGate(std::uint8_t vector) noexcept;

    const IdtGate64& Gate(std::uint8_t vector) const noexcept;
    bool IsPresent(std::uint8_t vector) const noexcept;

    std::uint16_t Limit() const noexcept;

private:
    std::array<IdtGate64, kEntryCount> entries_{};
};

} // namespace myps5emu::x86
