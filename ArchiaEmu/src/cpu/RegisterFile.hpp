#pragma once

#include <array>
#include <cstdint>

namespace myps5emu {

enum class Register : std::uint8_t {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8  = 8,
    R9  = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

class RegisterFile {
public:
    std::uint64_t Read64(std::uint8_t index) const noexcept;
    void Write64(std::uint8_t index, std::uint64_t value) noexcept;

    std::uint32_t Read32(std::uint8_t index) const noexcept;
    void Write32(std::uint8_t index, std::uint32_t value) noexcept;

    std::uint16_t Read16(std::uint8_t index) const noexcept;
    void Write16(std::uint8_t index, std::uint16_t value) noexcept;

    std::uint64_t Rax() const noexcept;
    std::uint64_t Rsp() const noexcept;

    void SetRax(std::uint64_t value) noexcept;
    void SetRsp(std::uint64_t value) noexcept;

private:
    std::array<std::uint64_t, 16> registers_{};
};

} // namespace myps5emu