#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

namespace {

bool Write32(Memory& memory, std::uint64_t address, std::uint32_t value)
{
    return memory.Write(
        address,
        reinterpret_cast<const std::uint8_t*>(&value),
        sizeof(value));
}

bool Run(Memory& memory, Cpu& cpu)
{
    cpu.ConnectMemory(&memory);
    cpu.SetInstructionPointer(0x1000);
    return cpu.Run() == 0;
}

} // namespace

int main()
{
    Memory memory;
    if (!memory.Map(
            0x1000,
            0x1000,
            MemoryPermission::Read |
            MemoryPermission::Write |
            MemoryPermission::Execute) ||
        !memory.Map(
            0x2000,
            0x1000,
            MemoryPermission::Read |
            MemoryPermission::Write)) {
        return 1;
    }

    // 67h switches a 64-bit code segment to 32-bit effective addresses.
    // The high half of RAX must be ignored when it is used as the base.
    const std::uint8_t baseCode[] = {
        0x67, 0x8B, 0x00, // MOV EAX, [EAX]
        0xF4
    };
    if (!memory.Write(0x1000, baseCode, sizeof(baseCode)) ||
        !Write32(memory, 0x2000, 0x11223344U)) {
        return 2;
    }

    Cpu baseCpu;
    baseCpu.WriteRegister64(0, 0x0000000100002000ULL);
    if (!Run(memory, baseCpu) || baseCpu.Rax() != 0x0000000011223344ULL) {
        std::cerr << "32-bit address-size override ignored high base bits\n";
        return 3;
    }

    // The SIB form must also perform all arithmetic at 32-bit width and
    // truncate the final effective address to 32 bits.
    const std::uint8_t sibCode[] = {
        0x67, 0x8B, 0x44, 0x88, 0x04, // MOV EAX, [EAX + ECX*4 + 4]
        0xF4
    };
    if (!memory.Write(0x1000, sibCode, sizeof(sibCode)) ||
        !Write32(memory, 0x2010, 0x55667788U)) {
        return 4;
    }

    Cpu sibCpu;
    sibCpu.WriteRegister64(0, 0x0000000100002000ULL);
    sibCpu.WriteRegister64(1, 0x0000000100000003ULL);
    if (!Run(memory, sibCpu) || sibCpu.Rax() != 0x0000000055667788ULL) {
        std::cerr << "32-bit SIB address-size override failed\n";
        return 5;
    }

    std::cout << "x86 address-size override test: PASS\n";
    return 0;
}
