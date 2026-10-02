#include "ExceptionTarget.hpp"

namespace myps5emu::x86 {

ExceptionTargetResolver::ExceptionTargetResolver(const Gdt64& gdt) noexcept
    : gdt_(gdt)
{
}

ExceptionTargetResult ExceptionTargetResolver::Resolve(
    const IdtGate64& gate,
    std::uint8_t current_cpl) const noexcept
{
    ExceptionTargetResult result{};

    // A null selector is defined by index 0; RPL bits do not make it a
    // non-null selector.  The resolver must therefore classify 0x0001,
    // 0x0002, and 0x0003 as null selectors as well.
    if ((gate.selector >> 3) == 0) {
        result.status = ExceptionTargetStatus::NullSelector;
        return result;
    }

    if ((gate.selector & 0x4U) != 0U) {
        result.status = ExceptionTargetStatus::LdtSelector;
        return result;
    }

    GdtCodeSegment64 segment{};
    if (!gdt_.ResolveCodeSegment(gate.selector, segment)) {
        result.status = ExceptionTargetStatus::NotPresent;
        return result;
    }

    result.segment = segment;

    if (!segment.present) {
        result.status = ExceptionTargetStatus::NotPresent;
        return result;
    }

    if (!segment.long_mode || segment.default_operand_size_32 ||
        segment.conforming) {
        result.status = ExceptionTargetStatus::InvalidLongModeSegment;
        return result;
    }

    if (segment.dpl > current_cpl) {
        result.status = ExceptionTargetStatus::PrivilegeViolation;
        return result;
    }

    result.status = ExceptionTargetStatus::Valid;
    return result;
}

} // namespace myps5emu::x86
