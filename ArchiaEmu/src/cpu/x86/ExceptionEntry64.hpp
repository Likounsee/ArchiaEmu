#pragma once

#include <cstdint>

#include "cpu/Cpu.hpp"
#include "ExceptionDelivery.hpp"
#include "ExceptionEntryWriter64.hpp"

namespace myps5emu::x86 {

enum class ExceptionEntryStatus : std::uint8_t {
    Delivered = 0,
    InvalidDelivery,
    StackUnderflow,
    Unmapped,
    PermissionDenied,
    MemoryFailure
};

struct ExceptionEntryResult {
    ExceptionEntryStatus status = ExceptionEntryStatus::InvalidDelivery;
    std::uint64_t new_rsp = 0;
    MemoryFault fault = MemoryFault::None;
};

class ExceptionEntry64 {
public:
    static ExceptionEntryResult Deliver(
        Cpu& cpu,
        Memory& memory,
        const ExceptionDeliveryResult& delivery) noexcept;
};

} // namespace myps5emu::x86
