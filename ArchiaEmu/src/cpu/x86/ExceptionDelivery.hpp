#pragma once

#include <cstdint>

#include "cpu/CpuException.hpp"
#include "ExceptionFrame64.hpp"
#include "ExceptionTarget.hpp"
#include "Idt.hpp"
#include "Tss64.hpp"

namespace myps5emu::x86 {

enum class ExceptionDeliveryStatus : std::uint8_t {
    Delivered = 0,
    NoVector,
    NotPresent,
    InvalidGate,
    InvalidTarget,
    StackUnavailable,
    InvalidStack
};

struct ExceptionDeliveryResult {
    ExceptionDeliveryStatus status = ExceptionDeliveryStatus::NoVector;
    ExceptionFrame64 frame{};
    std::uint64_t target_rip = 0;
    std::uint16_t target_cs = 0;
    std::uint64_t target_rflags = 0;
    GdtCodeSegment64 target_segment{};
    ExceptionStackSelection stack{};
};

class ExceptionDeliveryResolver {
public:
    ExceptionDeliveryResolver(
        const Idt& idt,
        const Gdt64& gdt,
        const Tss64& tss) noexcept;

    ExceptionDeliveryResult Resolve(
        const CpuException& exception,
        std::uint16_t current_cs,
        std::uint64_t current_rflags,
        std::uint8_t current_cpl,
        std::uint64_t error_code = 0) const noexcept;

private:
    const Idt& idt_;
    const Gdt64& gdt_;
    const Tss64& tss_;
};

} // namespace myps5emu::x86
