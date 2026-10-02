#include "cpu/x86/ExceptionTarget.hpp"

#include <iostream>

using namespace myps5emu::x86;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

GdtCodeSegment64 KernelCode()
{
    GdtCodeSegment64 segment{};
    segment.present = true;
    segment.long_mode = true;
    segment.dpl = 0;
    return segment;
}

} // namespace

int main()
{
    Gdt64 gdt;
    if (!gdt.SetCodeSegment(5, KernelCode())) {
        return Fail("Could not install valid 64-bit code segment") ? 0 : 1;
    }

    IdtGate64 gate{};
    gate.present = true;
    gate.selector = 5U << 3;
    gate.offset = 0xFFFF800000001000ULL;

    ExceptionTargetResolver resolver(gdt);

    const auto valid = resolver.Resolve(gate, 0);
    if (valid.status != ExceptionTargetStatus::Valid ||
        valid.segment.dpl != 0) {
        return Fail("Valid 64-bit exception target was rejected") ? 0 : 1;
    }

    for (const std::uint16_t nullSelector : {0x0000U, 0x0001U, 0x0002U, 0x0003U, 0x0007U}) {
        gate.selector = nullSelector;
        if (resolver.Resolve(gate, 0).status !=
            ExceptionTargetStatus::NullSelector) {
            return Fail("Null selector with index 0 was not classified correctly") ? 0 : 1;
        }
    }

    gate.selector = static_cast<std::uint16_t>((5U << 3) | 0x4U);
    if (resolver.Resolve(gate, 0).status !=
        ExceptionTargetStatus::LdtSelector) {
        return Fail("LDT selector was not rejected") ? 0 : 1;
    }

    GdtCodeSegment64 bad{};
    bad.present = true;
    bad.long_mode = false;
    bad.dpl = 0;
    if (gdt.SetCodeSegment(6, bad)) {
        return Fail("Invalid non-long-mode segment was accepted") ? 0 : 1;
    }

    bad = KernelCode();
    bad.dpl = 3;
    if (!gdt.SetCodeSegment(7, bad)) {
        return Fail("Valid user code segment could not be installed") ? 0 : 1;
    }

    gate.selector = 7U << 3;
    if (resolver.Resolve(gate, 0).status !=
        ExceptionTargetStatus::PrivilegeViolation) {
        return Fail("Privilege transition validation failed") ? 0 : 1;
    }

    if (resolver.Resolve(gate, 3).status != ExceptionTargetStatus::Valid) {
        return Fail("Same-level user exception target was rejected") ? 0 : 1;
    }

    std::cout << "x86-64 GDT exception target test: PASS\n";

    GdtDataSegment64 stack{};
    stack.present = true;
    stack.writable = true;
    stack.dpl = 3;
    if (!gdt.SetDataSegment(7, stack)) return 1;

    GdtDataSegment64 resolvedStack{};
    if (!gdt.ResolveDataSegment(0x3B, resolvedStack)) return 1;
    if (resolvedStack.dpl != 3) return 1;
    if (!resolvedStack.writable) return 1;
    return 0;
}
