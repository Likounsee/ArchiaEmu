#pragma once

#include <cstdint>

#include "cpu/CpuException.hpp"
#include "cpu/x86/Idt.hpp"

namespace myps5emu::x86 {

enum class IdtDispatchStatus : std::uint8_t {
    Delivered = 0,
    NoVector,
    NotPresent,
    InvalidGate
};

struct IdtDispatchResult {
    IdtDispatchStatus status = IdtDispatchStatus::NoVector;
    IdtGate64 gate{};
    std::uint8_t vector = 0;
};

class ExceptionDispatcher {
public:
    explicit ExceptionDispatcher(const Idt& idt) noexcept;

    IdtDispatchResult Resolve(const CpuException& exception) const noexcept;

private:
    const Idt& idt_;
};

} // namespace myps5emu::x86
