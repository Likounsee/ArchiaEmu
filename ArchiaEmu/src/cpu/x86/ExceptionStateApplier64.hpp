#pragma once

#include <cstdint>

#include "cpu/Cpu.hpp"
#include "ExceptionDelivery.hpp"

namespace myps5emu::x86 {

enum class ExceptionStateApplyStatus : std::uint8_t {
    Applied = 0,
    InvalidDelivery
};

struct ExceptionStateApplyResult {
    ExceptionStateApplyStatus status =
        ExceptionStateApplyStatus::InvalidDelivery;
};

class ExceptionStateApplier64 {
public:
    static ExceptionStateApplyResult Apply(
        Cpu& cpu,
        const ExceptionDeliveryResult& delivery,
        std::uint64_t new_rsp) noexcept;
};

} // namespace myps5emu::x86
