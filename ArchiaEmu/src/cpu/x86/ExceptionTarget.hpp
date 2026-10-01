#pragma once

#include <cstdint>

#include "Gdt.hpp"
#include "Idt.hpp"

namespace myps5emu::x86 {

enum class ExceptionTargetStatus : std::uint8_t {
    Valid = 0,
    NullSelector,
    LdtSelector,
    NotPresent,
    NotCodeSegment,
    InvalidLongModeSegment,
    PrivilegeViolation
};

struct ExceptionTargetResult {
    ExceptionTargetStatus status = ExceptionTargetStatus::NullSelector;
    GdtCodeSegment64 segment{};
};

class ExceptionTargetResolver {
public:
    explicit ExceptionTargetResolver(const Gdt64& gdt) noexcept;

    ExceptionTargetResult Resolve(
        const IdtGate64& gate,
        std::uint8_t current_cpl) const noexcept;

private:
    const Gdt64& gdt_;
};

} // namespace myps5emu::x86
