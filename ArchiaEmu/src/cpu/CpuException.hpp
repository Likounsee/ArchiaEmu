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

enum class CpuExceptionVector : std::uint8_t {
    None = 0xFF,
    DivideError = 0,
    InvalidOpcode = 6,
    PageFault = 14
};

struct CpuException {
    CpuExceptionKind kind = CpuExceptionKind::None;
    std::uint64_t instruction_pointer = 0;
    MemoryFault memory_fault = MemoryFault::None;
    CpuExceptionVector vector = CpuExceptionVector::None;
};

} // namespace myps5emu
