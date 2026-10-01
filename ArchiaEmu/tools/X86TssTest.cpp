#include "cpu/x86/Tss64.hpp"

#include <iostream>

using namespace myps5emu::x86;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    Tss64 tss;

    if (tss.Rsp0() != 0 || tss.Rsp1() != 0 || tss.Rsp2() != 0 ||
        tss.Ist(0) != 0 || tss.Ist(8) != 0) {
        return Fail("Default TSS stack pointers are not zero") ? 0 : 1;
    }

    tss.SetRsp0(0x1000);
    tss.SetRsp1(0x2000);
    tss.SetRsp2(0x3000);
    if (tss.Rsp0() != 0x1000 || tss.Rsp1() != 0x2000 ||
        tss.Rsp2() != 0x3000) {
        return Fail("RSP0/RSP1/RSP2 storage failed") ? 0 : 1;
    }

    for (std::uint8_t i = 1; i <= Tss64::kIstCount; ++i) {
        const std::uint64_t value = 0x7000 + (static_cast<std::uint64_t>(i) * 0x1000);
        if (!tss.SetIst(i, value) || tss.Ist(i) != value) {
            return Fail("IST storage failed") ? 0 : 1;
        }
    }

    if (tss.SetIst(0, 0x1234) || tss.SetIst(8, 0x1234)) {
        return Fail("Invalid IST index was accepted") ? 0 : 1;
    }

    ExceptionStackResolver resolver(tss);

    auto selection = resolver.ResolveIst(0);
    if (selection.status != ExceptionStackStatus::NoStackSwitch ||
        selection.stack_pointer != 0) {
        return Fail("IST=0 did not preserve no-stack-switch semantics") ? 0 : 1;
    }

    selection = resolver.ResolveIst(3);
    if (selection.status != ExceptionStackStatus::StackSelected ||
        selection.stack_pointer != tss.Ist(3)) {
        return Fail("IST3 resolution failed") ? 0 : 1;
    }

    selection = resolver.ResolveIst(8);
    if (selection.status != ExceptionStackStatus::InvalidIst) {
        return Fail("Out-of-range IST was not rejected") ? 0 : 1;
    }

    tss.SetIst(5, 0);
    selection = resolver.ResolveIst(5);
    if (selection.status != ExceptionStackStatus::Unavailable ||
        selection.stack_pointer != 0) {
        return Fail("Unavailable IST stack was not rejected") ? 0 : 1;
    }

    tss.SetIst(5, 0xB000);
    selection = resolver.Resolve(5, 3, 0);
    if (selection.status != ExceptionStackStatus::StackSelected ||
        selection.stack_pointer != 0xB000) {
        return Fail("IST stack did not override privilege stack") ? 0 : 1;
    }

    selection = resolver.Resolve(0, 3, 0);
    if (selection.status != ExceptionStackStatus::StackSelected ||
        selection.stack_pointer != 0x1000) {
        return Fail("RSP0 privilege transition failed") ? 0 : 1;
    }

    selection = resolver.Resolve(0, 3, 1);
    if (selection.status != ExceptionStackStatus::StackSelected ||
        selection.stack_pointer != 0x2000) {
        return Fail("RSP1 privilege transition failed") ? 0 : 1;
    }

    selection = resolver.Resolve(0, 3, 2);
    if (selection.status != ExceptionStackStatus::StackSelected ||
        selection.stack_pointer != 0x3000) {
        return Fail("RSP2 privilege transition failed") ? 0 : 1;
    }

    selection = resolver.Resolve(0, 0, 0);
    if (selection.status != ExceptionStackStatus::NoStackSwitch) {
        return Fail("Same-CPL exception incorrectly switched stacks") ? 0 : 1;
    }

    selection = resolver.Resolve(0, 4, 0);
    if (selection.status != ExceptionStackStatus::InvalidPrivilegeLevel) {
        return Fail("Invalid current CPL was accepted") ? 0 : 1;
    }

    tss.SetRsp0(0);
    selection = resolver.Resolve(0, 3, 0);
    if (selection.status != ExceptionStackStatus::Unavailable) {
        return Fail("Unavailable RSP0 was not rejected") ? 0 : 1;
    }

    std::cout << "x86-64 TSS/IST stack selection test: PASS\n";
    return 0;
}
