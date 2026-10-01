#pragma once

#include <cstdint>

#include "ExceptionStackFrame64.hpp"
#include "memory/Memory.hpp"

namespace myps5emu::x86 {

enum class ExceptionStackWriteStatus : std::uint8_t {
    Written = 0,
    StackUnderflow,
    Unmapped,
    PermissionDenied,
    MemoryFailure
};

struct ExceptionStackWriteResult {
    ExceptionStackWriteStatus status = ExceptionStackWriteStatus::MemoryFailure;
    std::uint64_t new_rsp = 0;
    MemoryFault fault = MemoryFault::None;
};

class ExceptionStackWriter64 {
public:
    static ExceptionStackWriteResult Write(
        Memory& memory,
        std::uint64_t stack_top,
        const ExceptionStackFrame64& frame) noexcept;
};

} // namespace myps5emu::x86
