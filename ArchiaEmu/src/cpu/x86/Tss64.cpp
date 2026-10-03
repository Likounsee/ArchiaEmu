#include "Tss64.hpp"

namespace myps5emu::x86 {

namespace {
bool IsCanonical48(std::uint64_t value) noexcept
{
    const std::uint64_t upper = value >> 48U;
    const bool sign = (value & (1ULL << 47U)) != 0;
    return upper == (sign ? 0xFFFFULL : 0ULL);
}
}

bool Tss64::SetRsp0(std::uint64_t value) noexcept
{
    rsp0_ = value;
    return true;
}

bool Tss64::SetRsp1(std::uint64_t value) noexcept
{
    rsp1_ = value;
    return true;
}

bool Tss64::SetRsp2(std::uint64_t value) noexcept
{
    rsp2_ = value;
    return true;
}

std::uint64_t Tss64::Rsp0() const noexcept
{
    return rsp0_;
}

std::uint64_t Tss64::Rsp1() const noexcept
{
    return rsp1_;
}

std::uint64_t Tss64::Rsp2() const noexcept
{
    return rsp2_;
}

bool Tss64::SetIst(std::uint8_t index, std::uint64_t value) noexcept
{
    if (index == 0 || index > kIstCount) {
        return false;
    }

    ist_[index - 1] = value;
    return true;
}

std::uint64_t Tss64::Ist(std::uint8_t index) const noexcept
{
    if (index == 0 || index > kIstCount) {
        return 0;
    }

    return ist_[index - 1];
}

ExceptionStackResolver::ExceptionStackResolver(const Tss64& tss) noexcept
    : tss_(tss)
{
}

ExceptionStackSelection ExceptionStackResolver::ResolveIst(
    std::uint8_t ist) const noexcept
{
    if (ist == 0) {
        return {ExceptionStackStatus::NoStackSwitch, 0};
    }

    if (ist > Tss64::kIstCount) {
        return {ExceptionStackStatus::InvalidIst, 0};
    }

    const auto stack_pointer = tss_.Ist(ist);
    if (stack_pointer == 0 || !IsCanonical48(stack_pointer)) {
        return {ExceptionStackStatus::Unavailable, 0};
    }

    return {ExceptionStackStatus::StackSelected, stack_pointer};
}

ExceptionStackSelection ExceptionStackResolver::Resolve(
    std::uint8_t ist,
    std::uint8_t current_cpl,
    std::uint8_t target_cpl) const noexcept
{
    if (current_cpl > 3 || target_cpl > 3) {
        return {ExceptionStackStatus::InvalidPrivilegeLevel, 0};
    }

    if (ist != 0) {
        return ResolveIst(ist);
    }

    if (target_cpl >= current_cpl) {
        return {ExceptionStackStatus::NoStackSwitch, 0};
    }

    std::uint64_t stack_pointer = 0;
    switch (target_cpl) {
    case 0:
        stack_pointer = tss_.Rsp0();
        break;
    case 1:
        stack_pointer = tss_.Rsp1();
        break;
    case 2:
        stack_pointer = tss_.Rsp2();
        break;
    default:
        return {ExceptionStackStatus::InvalidPrivilegeLevel, 0};
    }

    if (stack_pointer == 0) {
        return {ExceptionStackStatus::Unavailable, 0};
    }

    return {ExceptionStackStatus::StackSelected, stack_pointer};
}

} // namespace myps5emu::x86
