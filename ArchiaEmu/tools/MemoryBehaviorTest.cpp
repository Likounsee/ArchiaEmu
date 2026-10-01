#include "memory/Memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;

int main()
{
    Memory memory;
    if (!memory.Map(0x1000, 0x1000, MemoryPermission::Read | MemoryPermission::Write) ||
        !memory.Map(0x2000, 0x1000, MemoryPermission::Read | MemoryPermission::Write)) return 1;
    const std::array<std::uint8_t, 2> input{0x12,0x34};
    if (!memory.Write(0x1FFF, input.data(), input.size())) return 2;
    std::array<std::uint8_t, 2> output{};
    if (!memory.Read(0x1FFF, output.data(), output.size()) || output != input) return 3;
    if (!memory.Protect(0x1000, 0x1000, MemoryPermission::Read)) return 4;
    if (memory.Write(0x1000, input.data(), input.size())) return 5;
    if (!memory.Write(0x2000, input.data(), input.size())) return 6;
    if (!memory.Protect(0x1000, 0x1000, MemoryPermission::Read | MemoryPermission::Write)) return 7;
    if (!memory.Write(0x1000, input.data(), input.size())) return 8;
    std::cout << "Memory cross-region/protect test: PASS\n";
    return 0;
}
