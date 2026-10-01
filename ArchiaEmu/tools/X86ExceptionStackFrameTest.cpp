#include "cpu/x86/ExceptionStackFrame64.hpp"
#include <array>
#include <iostream>

using namespace myps5emu::x86;

int main()
{
    ExceptionStackFrame64 frame{};
    frame.rip = 0x1122334455667788ULL;
    frame.cs = 0x28;
    frame.rflags = 0x202;
    frame.privilege_stack_switch = true;
    frame.rsp = 0x9000;
    frame.ss = 0x30;
    frame.has_error_code = true;
    frame.error_code = 0x5;

    std::array<std::uint8_t, 48> bytes{};
    if (frame.QwordCount() != 6 || !frame.Encode(bytes.data(), bytes.size())) {
        std::cerr << "Frame encoding failed\n";
        return 1;
    }

    const std::array<std::uint8_t, 48> expected = {
        0x05,0,0,0,0,0,0,0,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,
        0x28,0,0,0,0,0,0,0,
        0x02,0x02,0,0,0,0,0,0,
        0x00,0x90,0,0,0,0,0,0,
        0x30,0,0,0,0,0,0,0
    };
    if (bytes != expected) {
        std::cerr << "Unexpected exception frame encoding\n";
        return 1;
    }

    if (frame.Encode(nullptr, bytes.size()) ||
        frame.Encode(bytes.data(), 8)) {
        std::cerr << "Invalid output buffer was accepted\n";
        return 1;
    }

    frame.has_error_code = false;
    if (frame.QwordCount() != 5) {
        std::cerr << "Same-CPL frame size is wrong\n";
        return 1;
    }

    std::cout << "x86-64 exception stack frame test: PASS\n";
    return 0;
}
