#pragma once

#include <cstdint>

#include "cpu/Cpu.hpp"
#include "Gdt.hpp"

namespace myps5emu::x86 {

enum class ExceptionReturnStatus : std::uint8_t {
    Returned = 0,
    Unmapped,
    PermissionDenied,
    InvalidFrame,
    InvalidInstructionPointer,
    InvalidCodeSegment,
    InvalidRflags,
    InvalidStack,
    MemoryFailure
};

struct ExceptionReturnResult {
    ExceptionReturnStatus status = ExceptionReturnStatus::InvalidFrame;
    std::uint64_t rip = 0;
    std::uint16_t cs = 0;
    std::uint64_t rflags = 0;
    std::uint64_t rsp = 0;
    std::uint16_t ss = 0;
};

class ExceptionReturn64 {
public:
    static ExceptionReturnResult Read(
        const Cpu& cpu,
        const Memory& memory,
        const Gdt64& gdt) noexcept;

    static ExceptionReturnResult Apply(
        Cpu& cpu,
        const ExceptionReturnResult& result) noexcept;
};

} // namespace myps5emu::x86
