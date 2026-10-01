#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace myps5emu::x86 {

struct GdtDataSegment64 {
    bool present = false;
    bool writable = false;
    std::uint8_t dpl = 0;

    bool IsValidLongModeStackSegment() const noexcept;
};

struct GdtCodeSegment64 {
    bool present = false;
    bool conforming = false;
    bool long_mode = false;
    bool default_operand_size_32 = false;
    std::uint8_t dpl = 0;

    bool IsValidLongModeTarget() const noexcept;
};

class Gdt64 {
public:
    static constexpr std::size_t kMaxEntries = 8192;
    static constexpr std::uint16_t kMaxLimit =
        static_cast<std::uint16_t>(kMaxEntries * 8 - 1);

    Gdt64() noexcept;

    bool SetCodeSegment(std::uint16_t index,
                         const GdtCodeSegment64& segment) noexcept;
    bool SetDataSegment(std::uint16_t index,
                        const GdtDataSegment64& segment) noexcept;
    void Clear(std::uint16_t index) noexcept;

    const GdtCodeSegment64& Entry(std::uint16_t index) const noexcept;
    std::uint16_t Limit() const noexcept;

    bool ResolveCodeSegment(std::uint16_t selector,
                            GdtCodeSegment64& segment) const noexcept;
    bool ResolveDataSegment(std::uint16_t selector,
                            GdtDataSegment64& segment) const noexcept;

private:
    std::array<GdtCodeSegment64, kMaxEntries> code_entries_{};
    std::array<GdtDataSegment64, kMaxEntries> data_entries_{};
};

} // namespace myps5emu::x86
