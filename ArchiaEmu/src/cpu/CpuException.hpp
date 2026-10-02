#pragma once

#include <cstdint>

#include "memory/Memory.hpp"

namespace myps5emu {

enum class CpuExceptionKind : std::uint8_t {
    None = 0,
    MemoryFault,
    InvalidOpcode,
    DivideError,
    GeneralProtection,
    SoftwareInterrupt
};

enum class CpuExceptionVector : std::uint8_t {
    None = 0xFF,
    DivideError = 0,
    InvalidOpcode = 6,
    PageFault = 14,
    GeneralProtection = 13,
    Breakpoint = 3,
    Overflow = 4
};

struct CpuException {
    CpuExceptionKind kind = CpuExceptionKind::None;
    std::uint64_t instruction_pointer = 0;
    MemoryFault memory_fault = MemoryFault::None;
    CpuExceptionVector vector = CpuExceptionVector::None;
    std::uint64_t page_fault_address = 0;
    std::uint32_t page_fault_error = 0;
};

} // namespace myps5emu
