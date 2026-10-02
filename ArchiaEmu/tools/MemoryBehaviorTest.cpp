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

    std::array<std::uint8_t, 1> instruction{0xF4};
    if (memory.ExecuteRead(0x1000, instruction.data(), instruction.size())) return 13;
    if (!memory.Protect(0x1000, 0x1000,
                        MemoryPermission::Read | MemoryPermission::Execute)) {
        return 14;
    }
    if (!memory.ExecuteRead(0x1000, instruction.data(), instruction.size()) ||
        instruction[0] != input[0]) {
        return 15;
    }
    // Large console RAM must be virtual/sparse: mapping 16 GiB must not
    // allocate 16 GiB of host memory before the guest touches a page.
    Memory largeMemory;
    constexpr std::uint64_t consoleRamSize = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    if (!largeMemory.Map(0, consoleRamSize,
                         MemoryPermission::Read | MemoryPermission::Write)) {
        return 9;
    }

    const std::uint8_t markerByte = 0x5A;
    constexpr std::uint64_t highAddress = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    if (!largeMemory.Write(highAddress, &markerByte, 1)) {
        return 10;
    }

    std::uint8_t highRead = 0;
    if (!largeMemory.Read(highAddress, &highRead, 1) ||
        highRead != markerByte) {
        return 11;
    }

    std::uint8_t untouched = 0xFF;
    if (!largeMemory.Read(highAddress + 1, &untouched, 1) ||
        untouched != 0) {
        return 12;
    }

    std::cout << "Memory cross-region/protect test: PASS\n";
    return 0;
}
