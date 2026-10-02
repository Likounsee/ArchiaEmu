#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
#include <vector>

using namespace myps5emu;

static void AppendMovR64(std::vector<std::uint8_t>& code, std::uint8_t reg, std::uint64_t value) {
    code.push_back(static_cast<std::uint8_t>(0xB8U + (reg & 7U)));
    if (reg >= 8) {
        code.insert(code.end() - 1, 0x41);
    }
    for (unsigned i = 0; i < 8; ++i) code.push_back(static_cast<std::uint8_t>(value >> (i * 8U)));
}

static bool Run(Memory& memory, Cpu& cpu, std::vector<std::uint8_t> code) {
    code.push_back(0xF4);
    if (!memory.Write(0x1000, code.data(), code.size())) return false;
    cpu.SetInstructionPointer(0x1000);
    return cpu.Run() == 0;
}

static bool TestMovsxd() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 3, 0x00000000FFFFFFFFULL);
    code.insert(code.end(), {0x48, 0x63, 0xC3});
    return Run(memory, cpu, code) && cpu.Rax() == 0xFFFFFFFFFFFFFFFFULL;
}

static bool TestBswap() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0x1122334455667788ULL);
    code.insert(code.end(), {0x48, 0x0F, 0xC8});
    return Run(memory, cpu, code) && cpu.Rax() == 0x8877665544332211ULL;
}

static bool TestCmovz() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 1);
    AppendMovR64(code, 3, 0x1122334455667788ULL);
    code.insert(code.end(), {0x48, 0x39, 0xC0, 0x48, 0x0F, 0x44, 0xC3});
    return Run(memory, cpu, code) && cpu.Rax() == 0x1122334455667788ULL;
}

static bool TestXadd32() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 5);
    AppendMovR64(code, 3, 7);
    code.insert(code.end(), {0x0F, 0xC1, 0xC3});
    return Run(memory, cpu, code) && cpu.Rax() == 5 && cpu.ReadRegister64(3) == 12;
}

static bool TestXadd8() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 5);
    AppendMovR64(code, 3, 7);
    code.insert(code.end(), {0x0F, 0xC0, 0xC3});
    return Run(memory, cpu, code) && (cpu.ReadRegister64(0) & 0xFFU) == 5 && (cpu.ReadRegister64(3) & 0xFFU) == 12;
}

static bool TestCmpxchg64() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 10);
    AppendMovR64(code, 3, 20);
    AppendMovR64(code, 1, 20);
    code.insert(code.end(), {0x48, 0x0F, 0xB1, 0xC3});
    return Run(memory, cpu, code) &&
           cpu.ReadRegister64(0) == 20 &&
           cpu.ReadRegister64(3) == 20 &&
           (cpu.Rflags() & (1ULL << 6)) == 0;
}

static bool TestFlagsAndLoops() {
    Memory memory; memory.Map(0x1000, 0x2000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0x000000000000D500ULL);
    code.insert(code.end(), {0x9E, 0x9F});
    if (!Run(memory, cpu, code) || ((cpu.Rax() >> 8) & 0xFFU) != 0xD7U) return false;

    Memory memory2; memory2.Map(0x1000, 0x2000);
    Cpu cpu2; cpu2.ConnectMemory(&memory2);
    std::vector<std::uint8_t> flagsCode = {0xF8, 0xF9, 0xF5, 0xFC, 0xFD};
    if (!Run(memory2, cpu2, flagsCode)) return false;
    if ((cpu2.Rflags() & 1ULL) == 0 || (cpu2.Rflags() & (1ULL << 10)) == 0) return false;

    Memory memory3; memory3.Map(0x1000, 0x2000);
    Cpu cpu3; cpu3.ConnectMemory(&memory3);
    std::vector<std::uint8_t> loopCode;
    AppendMovR64(loopCode, 1, 2);
    loopCode.insert(loopCode.end(), {0xE2, 0xFE, 0xF4});
    if (!Run(memory3, cpu3, loopCode) || cpu3.ReadRegister64(1) != 0) return false;

    Memory memory4; memory4.Map(0x1000, 0x2000);
    Cpu cpu4; cpu4.ConnectMemory(&memory4);
    std::vector<std::uint8_t> jrcxzCode;
    AppendMovR64(jrcxzCode, 1, 0);
    jrcxzCode.insert(jrcxzCode.end(), {0xE3, 0x01, 0x90, 0xF4});
    return Run(memory4, cpu4, jrcxzCode);
}

static bool TestCpuid() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0);
    AppendMovR64(code, 1, 0);
    code.insert(code.end(), {0x0F, 0xA2});
    return Run(memory, cpu, code) && cpu.Rax() >= 1;
}

int main() {
    if (!TestMovsxd()) { std::cerr << "MOVSXD failed\n"; return 1; }
    if (!TestBswap()) { std::cerr << "BSWAP failed\n"; return 2; }
    if (!TestCmovz()) { std::cerr << "CMOVZ failed\n"; return 3; }
    if (!TestXadd32()) { std::cerr << "XADD failed\n"; return 4; }
    if (!TestXadd8()) { std::cerr << "XADD8 failed\n"; return 5; }
    if (!TestCmpxchg64()) { std::cerr << "CMPXCHG failed\n"; return 6; }
    if (!TestFlagsAndLoops()) { std::cerr << "flags/loops failed\n"; return 6; }
    if (!TestCpuid()) { std::cerr << "CPUID failed\n"; return 7; }
    std::cout << "x86 extended integer instruction test: PASS\n";
    return 0;
}
