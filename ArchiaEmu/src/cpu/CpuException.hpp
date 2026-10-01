#pragma once

#include <cstdint>

#include "memory/Memory.hpp"

namespace myps5emu {

enum class CpuExceptionKind : std::uint8_t {
    None = 0,
    MemoryFault,
    InvalidOpcode,
    DivideError
};

struct CpuException {
    CpuExceptionKind kind = CpuExceptionKind::None;
    std::uint64_t instruction_pointer = 0;
    MemoryFault memory_fault = MemoryFault::None;
};

} // namespace myps5emu
