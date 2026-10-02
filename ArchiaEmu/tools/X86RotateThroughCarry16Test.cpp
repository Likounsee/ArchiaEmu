#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>

using namespace myps5emu;

static bool Run(Memory& memory, Cpu& cpu, const std::uint8_t* code, std::size_t size) {
    if (!memory.Write(0x1000, code, size)) return false;
    cpu.SetInstructionPointer(0x1000);
    for (int i = 0; i < 8; ++i) {
        if (cpu.Step() != 0) return false;
        if (cpu.Rip() >= 0x1000 + size) return true;
    }
    return false;
}

int main() {
    Memory memory;
    memory.Map(0x1000, 0x1000);

    {
        // 66 C1 /2, count=1: RCL AX,1 with CF=1.
        const std::uint8_t code[] = {0x66, 0xC1, 0xD0, 0x01, 0xF4};
        Cpu cpu;
        cpu.WriteRegister64(0, 0x1122334455668000ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334455660001ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "16-bit RCL failed\n";
            return 1;
        }
    }

    {
        // 66 C1 /3, count=1: RCR AX,1 with CF=1.
        const std::uint8_t code[] = {0x66, 0xC1, 0xD8, 0x01, 0xF4};
        Cpu cpu;
        cpu.WriteRegister64(0, 0x1122334455660001ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334455668000ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "16-bit RCR failed\n";
            return 2;
        }
    }

    std::cout << "x86 16-bit rotate-through-carry test: PASS\n";
    return 0;
}
