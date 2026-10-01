#pragma once

#include <cstdint>

#include "cpu/CpuException.hpp"

namespace myps5emu::x86 {

struct ExceptionFrame64 {
    std::uint64_t rip = 0;
    std::uint64_t cs = 0;
    std::uint64_t rflags = 0;
    bool has_error_code = false;
    std::uint64_t error_code = 0;

    static ExceptionFrame64 Build(
        const CpuException& exception,
        std::uint64_t saved_cs,
        std::uint64_t saved_rflags,
        std::uint64_t error_code = 0) noexcept;

    static bool HasHardwareErrorCode(
        CpuExceptionVector vector) noexcept;
};

} // namespace myps5emu::x86
