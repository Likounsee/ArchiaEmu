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

    {
        const std::uint8_t code[] = {0xC1, 0xD0, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x0000000080000000ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000000000001ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "32-bit RCL failed\n";
            return 3;
        }
    }

    {
        const std::uint8_t code[] = {0xC1, 0xD8, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x1122334400000001ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334480000000ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "32-bit RCR failed\n";
            return 4;
        }
    }

    {
        const std::uint8_t code[] = {0x48, 0xC1, 0xD0, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x8000000000000000ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000000000001ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "64-bit RCL failed\n";
            return 5;
        }
    }

    {
        const std::uint8_t code[] = {0x48, 0xC1, 0xD8, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x0000000000000001ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x8000000000000000ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "64-bit RCR failed\n";
            return 6;
        }
    }

    {
        const std::uint8_t code[] = {0xC0, 0xD0, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x1122334455660080ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334455660001ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "8-bit RCL failed\n";
            return 7;
        }
    }

    {
        const std::uint8_t code[] = {0xC0, 0xD8, 0x01};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x1122334455660001ULL);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334455660080ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "8-bit RCR failed\n";
            return 8;
        }
    }

    {
        const std::uint8_t code[] = {0xD2, 0xD0};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x1122334455660080ULL);
        cpu.WriteRegister64(1, 1);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x1122334455660001ULL) {
            std::cerr << "8-bit RCL CL failed\n";
            return 9;
        }
    }

    {
        const std::uint8_t code[] = {0x48, 0xD3, 0xD0};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x8000000000000000ULL);
        cpu.WriteRegister64(1, 1);
        cpu.SetRflags(cpu.Rflags() | (1ULL << 0));
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000000000001ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "64-bit RCL CL failed\n";
            return 10;
        }
    }

    std::cout << "x86 rotate-through-carry test: PASS\n";
    return 0;
}
