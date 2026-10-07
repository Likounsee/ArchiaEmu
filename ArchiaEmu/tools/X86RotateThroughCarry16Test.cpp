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
    memory.Map(0x2000, 0x1000);

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
            cpu.ReadRegister64(0) != 0x0000000080000000ULL ||
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

    // A zero effective count must leave both the operand and all flags intact.
    {
        const std::uint8_t code[] = {0x66, 0xC1, 0xD0, 0x00};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        constexpr std::uint64_t initial = 0x112233445566A55AULL;
        constexpr std::uint64_t flags = 0x8D5ULL;
        cpu.WriteRegister64(0, initial);
        cpu.SetRflags(flags);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != initial || cpu.Rflags() != flags) {
            std::cerr << "16-bit RCL zero-count failed\n";
            return 11;
        }
    }

    // For RCL16, a count of 17 is equivalent to zero because the rotate
    // width includes CF (17 bits). This must also preserve flags.
    {
        const std::uint8_t code[] = {0x66, 0xC1, 0xD0, 0x11};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        constexpr std::uint64_t initial = 0x112233445566A55AULL;
        constexpr std::uint64_t flags = 0x8D5ULL;
        cpu.WriteRegister64(0, initial);
        cpu.SetRflags(flags);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != initial || cpu.Rflags() != flags) {
            std::cerr << "16-bit RCL count-17 masking failed\n";
            return 12;
        }
    }

    // For RCL32, count 33 is equivalent to one.
    {
        const std::uint8_t code[] = {0xC1, 0xD0, 0x21};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x0000000080000000ULL);
        cpu.SetRflags(0);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000000000000ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "32-bit RCL count-33 masking failed\n";
            return 13;
        }
    }

    // For RCL64, count 65 is equivalent to one.
    {
        const std::uint8_t code[] = {0x48, 0xC1, 0xD0, 0x41};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 0x8000000000000000ULL);
        cpu.SetRflags(0);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000000000000ULL ||
            (cpu.Rflags() & (1ULL << 0)) == 0) {
            std::cerr << "64-bit RCL count-65 masking failed\n";
            return 14;
        }
    }

    // RCR has the same rotate-through-carry count modulus.
    {
        const std::uint8_t code[] = {0x66, 0xC1, 0xD8, 0x11};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        constexpr std::uint64_t initial = 0x112233445566A55AULL;
        constexpr std::uint64_t flags = 0x8D5ULL;
        cpu.WriteRegister64(0, initial);
        cpu.SetRflags(flags);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != initial || cpu.Rflags() != flags) {
            std::cerr << "16-bit RCR count-17 masking failed\n";
            return 15;
        }
    }

    {
        const std::uint8_t code[] = {0xC1, 0xD8, 0x21};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 1ULL);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x0000000080000000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "32-bit RCR count-33 masking failed\n";
            return 16;
        }
    }

    {
        const std::uint8_t code[] = {0x48, 0xC1, 0xD8, 0x41};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(0, 1ULL);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != 0x8000000000000000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "64-bit RCR count-65 masking failed\n";
            return 17;
        }
    }

    // The 8-bit rotate-through-carry width is nine bits, so count 9 is a
    // true zero-count after the architectural modulo operation.
    {
        const std::uint8_t code[] = {0xC0, 0xD0, 0x09};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        constexpr std::uint64_t initial = 0x11223344556600A5ULL;
        constexpr std::uint64_t flags = 0x8D5ULL;
        cpu.WriteRegister64(0, initial);
        cpu.SetRflags(flags);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != initial || cpu.Rflags() != flags) {
            std::cerr << "8-bit RCL count-9 masking failed\n";
            return 18;
        }
    }

    {
        const std::uint8_t code[] = {0xC0, 0xD8, 0x09};
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        constexpr std::uint64_t initial = 0x112233445566005AU;
        constexpr std::uint64_t flags = 0x8D5ULL;
        cpu.WriteRegister64(0, initial);
        cpu.SetRflags(flags);
        if (!Run(memory, cpu, code, sizeof(code)) ||
            cpu.ReadRegister64(0) != initial || cpu.Rflags() != flags) {
            std::cerr << "8-bit RCR count-9 masking failed\n";
            return 19;
        }
    }

    // Memory destinations must honor the same width and REX.B decoding as
    // register forms. The base register itself must remain unchanged.
    {
        const std::uint64_t initial = 0x8000000000000000ULL;
        if (!memory.Write(0x2000, reinterpret_cast<const std::uint8_t*>(&initial), sizeof(initial))) return 20;
        const std::uint8_t code[] = {
            0x49, 0xC1, 0x13, 0x01 // RCL QWORD PTR [R11],1
        };
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(11, 0x2000);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code))) return 21;
        std::uint64_t result = 0;
        if (!memory.Read(0x2000, reinterpret_cast<std::uint8_t*>(&result), sizeof(result)) ||
            result != 0x0000000000000001ULL ||
            cpu.ReadRegister64(11) != 0x2000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "64-bit RCL memory failed\n";
            return 22;
        }
    }

    {
        const std::uint64_t initial = 1;
        if (!memory.Write(0x2000, reinterpret_cast<const std::uint8_t*>(&initial), sizeof(initial))) return 23;
        const std::uint8_t code[] = {
            0x49, 0xC1, 0x1B, 0x01 // RCR QWORD PTR [R11],1
        };
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(11, 0x2000);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code))) return 24;
        std::uint64_t result = 0;
        if (!memory.Read(0x2000, reinterpret_cast<std::uint8_t*>(&result), sizeof(result)) ||
            result != 0x8000000000000000ULL ||
            cpu.ReadRegister64(11) != 0x2000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "64-bit RCR memory failed\n";
            return 25;
        }
    }

    {
        const std::uint16_t initial = 0x8000U;
        if (!memory.Write(0x2000, reinterpret_cast<const std::uint8_t*>(&initial), sizeof(initial))) return 26;
        const std::uint8_t code[] = {
            0x66, 0x41, 0xC1, 0x13, 0x01 // RCL WORD PTR [R11],1
        };
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(11, 0x2000);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code))) return 27;
        std::uint16_t result = 0;
        if (!memory.Read(0x2000, reinterpret_cast<std::uint8_t*>(&result), sizeof(result)) ||
            result != 0x0001U || cpu.ReadRegister64(11) != 0x2000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "16-bit RCL memory failed\n";
            return 28;
        }
    }

    {
        const std::uint32_t initial = 1U;
        if (!memory.Write(0x2000, reinterpret_cast<const std::uint8_t*>(&initial), sizeof(initial))) return 29;
        const std::uint8_t code[] = {
            0x41, 0xC1, 0x1B, 0x01 // RCR DWORD PTR [R11],1
        };
        Cpu cpu;
        cpu.ConnectMemory(&memory);
        cpu.WriteRegister64(11, 0x2000);
        cpu.SetRflags(1ULL);
        if (!Run(memory, cpu, code, sizeof(code))) return 30;
        std::uint32_t result = 0;
        if (!memory.Read(0x2000, reinterpret_cast<std::uint8_t*>(&result), sizeof(result)) ||
            result != 0x80000000U || cpu.ReadRegister64(11) != 0x2000ULL ||
            (cpu.Rflags() & 1ULL) == 0) {
            std::cerr << "32-bit RCR memory failed\n";
            return 31;
        }
    }

    std::cout << "x86 rotate-through-carry test: PASS\n";
    return 0;
}
