#include "ExceptionStateApplier64.hpp"

namespace myps5emu::x86 {

ExceptionStateApplyResult ExceptionStateApplier64::Apply(
    Cpu& cpu,
    const ExceptionDeliveryResult& delivery,
    std::uint64_t new_rsp) noexcept
{
    ExceptionStateApplyResult result{};

    if (delivery.status != ExceptionDeliveryStatus::Delivered) {
        return result;
    }

    cpu.SetInstructionPointer(delivery.target_rip);
    cpu.SetCodeSegment(delivery.target_cs);
    cpu.SetStackPointer(new_rsp);
    cpu.SetStackSegment(delivery.target_ss);
    cpu.SetRflags(delivery.target_rflags);

    result.status = ExceptionStateApplyStatus::Applied;
    return result;
}

} // namespace myps5emu::x86
