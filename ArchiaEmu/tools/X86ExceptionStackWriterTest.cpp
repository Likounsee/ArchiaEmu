#include "cpu/x86/ExceptionStackWriter64.hpp"

#include <array>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

bool CheckFrame(Memory& memory, std::uint64_t address)
{
    std::array<std::uint8_t, 48> bytes{};
    if (!memory.Read(address, bytes.data(), bytes.size())) {
        return false;
    }

    const std::array<std::uint8_t, 48> expected = {
        0x05,0,0,0,0,0,0,0,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,
        0x28,0,0,0,0,0,0,0,
        0x02,0x02,0,0,0,0,0,0,
        0x00,0x70,0,0,0,0,0,0,
        0x18,0,0,0,0,0,0,0
    };
    return bytes == expected;
}

} // namespace

int main()
{
    Memory memory;
    if (!memory.Map(0x6000, 0x2000,
                    MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Failed to map writable stack") ? 0 : 1;
    }

    ExceptionStackFrame64 frame{};
    frame.rip = 0x1122334455667788ULL;
    frame.cs = 0x28;
    frame.rflags = 0x202;
    frame.rsp = 0x7000;
    frame.ss = 0x18;
    frame.has_error_code = true;
    frame.error_code = 0x5;

    const auto written = ExceptionStackWriter64::Write(memory, 0x7048, frame);
    if (written.status != ExceptionStackWriteStatus::Written ||
        written.new_rsp != 0x7000 ||
        written.fault != MemoryFault::None ||
        !CheckFrame(memory, written.new_rsp)) {
        return Fail("Exception frame was not written with the expected layout")
            ? 0 : 1;
    }

    Memory read_only;
    if (!read_only.Map(0x6000, 0x2000, MemoryPermission::Read)) {
        return Fail("Failed to map read-only stack") ? 0 : 1;
    }
    const auto denied =
        ExceptionStackWriter64::Write(read_only, 0x7048, frame);
    if (denied.status != ExceptionStackWriteStatus::PermissionDenied ||
        denied.fault != MemoryFault::PermissionDenied) {
        return Fail("Write to a read-only stack was not rejected")
            ? 0 : 1;
    }

    Memory unmapped;
    const auto missing =
        ExceptionStackWriter64::Write(unmapped, 0x7048, frame);
    if (missing.status != ExceptionStackWriteStatus::Unmapped ||
        missing.fault != MemoryFault::Unmapped) {
        return Fail("Write to an unmapped stack was not rejected")
            ? 0 : 1;
    }

    const auto underflow =
        ExceptionStackWriter64::Write(memory, 0x20, frame);
    if (underflow.status != ExceptionStackWriteStatus::StackUnderflow) {
        return Fail("Stack underflow was not rejected") ? 0 : 1;
    }

    std::cout << "x86-64 exception stack writer test: PASS\n";
    return 0;
}
