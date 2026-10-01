#include "ExceptionDispatcher.hpp"

namespace myps5emu::x86 {

ExceptionDispatcher::ExceptionDispatcher(const Idt& idt) noexcept
    : idt_(idt)
{
}

IdtDispatchResult ExceptionDispatcher::Resolve(
    const CpuException& exception) const noexcept
{
    IdtDispatchResult result{};

    if (exception.vector == CpuExceptionVector::None) {
        result.status = IdtDispatchStatus::NoVector;
        return result;
    }

    result.vector = static_cast<std::uint8_t>(exception.vector);
    result.gate = idt_.Gate(result.vector);

    if (!idt_.IsPresent(result.vector)) {
        result.status = IdtDispatchStatus::NotPresent;
        return result;
    }

    if (!result.gate.IsValid()) {
        result.status = IdtDispatchStatus::InvalidGate;
        return result;
    }

    result.status = IdtDispatchStatus::Delivered;
    return result;
}

} // namespace myps5emu::x86
