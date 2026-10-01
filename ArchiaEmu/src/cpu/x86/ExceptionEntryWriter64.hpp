#pragma once

#include <cstdint>

#include "ExceptionDelivery.hpp"
#include "ExceptionStackWriter64.hpp"

namespace myps5emu::x86 {

enum class ExceptionEntryWriteStatus : std::uint8_t {
    Written = 0,
    InvalidDelivery,
    StackUnderflow,
    Unmapped,
    PermissionDenied,
    MemoryFailure
};

struct ExceptionEntryWriteResult {
    ExceptionEntryWriteStatus status = ExceptionEntryWriteStatus::InvalidDelivery;
    std::uint64_t new_rsp = 0;
    MemoryFault fault = MemoryFault::None;
};

class ExceptionEntryWriter64 {
public:
    static ExceptionEntryWriteResult Write(
        Memory& memory,
        const ExceptionDeliveryResult& delivery) noexcept;
};

} // namespace myps5emu::x86
