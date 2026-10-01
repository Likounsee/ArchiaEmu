#include "RegisterFile.hpp"

namespace myps5emu {

std::uint64_t RegisterFile::Read64(std::uint8_t index) const noexcept
{
    if (index >= registers_.size()) {
        return 0;
    }

    return registers_[index];
}

void RegisterFile::Write64(
    std::uint8_t index,
    std::uint64_t value) noexcept
{
    if (index >= registers_.size()) {
        return;
    }

    registers_[index] = value;
}

std::uint32_t RegisterFile::Read32(std::uint8_t index) const noexcept
{
    return static_cast<std::uint32_t>(Read64(index));
}

void RegisterFile::Write32(
    std::uint8_t index,
    std::uint32_t value) noexcept
{
    // x86-64 zero-extends 32-bit register writes.
    Write64(index, static_cast<std::uint64_t>(value));
}

std::uint64_t RegisterFile::Rax() const noexcept
{
    return Read64(static_cast<std::uint8_t>(Register::RAX));
}

std::uint64_t RegisterFile::Rsp() const noexcept
{
    return Read64(static_cast<std::uint8_t>(Register::RSP));
}

void RegisterFile::SetRax(std::uint64_t value) noexcept
{
    Write64(
        static_cast<std::uint8_t>(Register::RAX),
        value);
}

void RegisterFile::SetRsp(std::uint64_t value) noexcept
{
    Write64(
        static_cast<std::uint8_t>(Register::RSP),
        value);
}

} // namespace myps5emu