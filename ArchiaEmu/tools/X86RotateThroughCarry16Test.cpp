#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
#include <vector>

using namespace myps5emu;

static bool Run(Memory& memory, Cpu& cpu, const std::uint8_t* code, std::size_t size) {
    std::vector<std::uint8_t> program(code, code + size);
    program.push_back(0xC3);
    if (!memory.Write(0x1000, program.data(), program.size())) return false;
    cpu.SetInstructionPointer(0x1000);
    return cpu.Run() == 0;
}

int main() {
    Memory memory;
    memory.Map(0x1000, 0x1000);

    {
        const std::uint8_t code[] = {0x66, 0xC1, 0xD0, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
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
        const std::uint8_t code[] = {0x66, 0xC1, 0xD8, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
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
