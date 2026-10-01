#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace myps5emu::x86 {

class Tss64 {
public:
    static constexpr std::size_t kIstCount = 7;

    bool SetRsp0(std::uint64_t value) noexcept;
    bool SetRsp1(std::uint64_t value) noexcept;
    bool SetRsp2(std::uint64_t value) noexcept;

    std::uint64_t Rsp0() const noexcept;
    std::uint64_t Rsp1() const noexcept;
    std::uint64_t Rsp2() const noexcept;

    bool SetIst(std::uint8_t index, std::uint64_t value) noexcept;
    std::uint64_t Ist(std::uint8_t index) const noexcept;

private:
    std::array<std::uint64_t, kIstCount> ist_{};
    std::uint64_t rsp0_ = 0;
    std::uint64_t rsp1_ = 0;
    std::uint64_t rsp2_ = 0;
};

enum class ExceptionStackStatus : std::uint8_t {
    NoStackSwitch = 0,
    StackSelected,
    InvalidIst,
    Unavailable,
    InvalidPrivilegeLevel
};

struct ExceptionStackSelection {
    ExceptionStackStatus status = ExceptionStackStatus::NoStackSwitch;
    std::uint64_t stack_pointer = 0;
};

class ExceptionStackResolver {
public:
    explicit ExceptionStackResolver(const Tss64& tss) noexcept;

    ExceptionStackSelection ResolveIst(std::uint8_t ist) const noexcept;

    ExceptionStackSelection Resolve(
        std::uint8_t ist,
        std::uint8_t current_cpl,
        std::uint8_t target_cpl) const noexcept;

private:
    const Tss64& tss_;
};

} // namespace myps5emu::x86
