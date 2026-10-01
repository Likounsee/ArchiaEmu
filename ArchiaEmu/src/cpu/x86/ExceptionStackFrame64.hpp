#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ExceptionFrame64.hpp"

namespace myps5emu::x86 {

struct ExceptionStackFrame64 {
    std::uint64_t rip = 0;
    std::uint64_t cs = 0;
    std::uint64_t rflags = 0;
    bool privilege_stack_switch = false;
    std::uint64_t rsp = 0;
    std::uint64_t ss = 0;
    bool has_error_code = false;
    std::uint64_t error_code = 0;

    std::size_t QwordCount() const noexcept;
    bool Encode(std::uint8_t* out, std::size_t size) const noexcept;
};

} // namespace myps5emu::x86
