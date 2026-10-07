#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;

static bool Run(Memory& memory, std::uint64_t flags, std::uint64_t initialRax,
                 const std::array<std::uint8_t,3>& code, std::uint64_t& resultRax,
                 std::uint64_t& resultFlags)
{
    if (!memory.Write(0x1000, code.data(), code.size())) return false;
    const std::uint8_t hlt = 0xF4;
    if (!memory.Write(0x1003, &hlt, 1)) return false;
    Cpu cpu;
    cpu.ConnectMemory(&memory);
    cpu.WriteRegister64(0, initialRax);
    cpu.SetInstructionPointer(0x1000);
    cpu.SetRflags(flags);
    if (cpu.Run() != 0) return false;
    resultRax = cpu.ReadRegister64(0);
    resultFlags = cpu.Rflags();
    return true;
}

int main()
{
    Memory memory;
    if (!memory.Map(0x1000,0x1000,MemoryPermission::Read|MemoryPermission::Write|MemoryPermission::Execute) ||
        !memory.Map(0x2000,0x1000,MemoryPermission::Read|MemoryPermission::Write)) return 1;
    std::uint64_t result = 0;
    std::uint64_t resultFlags = 0;
    // SETE AH: no REX, rm=4 means AH.
    if (!Run(memory,1ULL<<6,0x1234000000000000ULL,{0x0F,0x94,0xC4},result,resultFlags)) return 2;
    if (result != 0x1234000000000100ULL || resultFlags != (1ULL<<6)) return 3;
    // SETP AH: PF=1 must set AH and preserve the rest.
    if (!Run(memory,1ULL<<2,0x1234000000000000ULL,{0x0F,0x9A,0xC4},result,resultFlags)) return 4;
    if (result != 0x1234000000000100ULL || resultFlags != (1ULL<<2)) return 5;
    // SETE byte memory form.
    if (!Run(memory,1ULL<<6,0x2000,{0x0F,0x94,0x00},result,resultFlags)) return 6;
    std::uint8_t value=0;
    if (!memory.Read(0x2000,&value,1) || value != 1 || resultFlags != (1ULL<<6)) return 7;

    // Exercise every SETcc condition in the architectural condition-code
    // matrix using AL as the byte destination.
    struct Case { std::uint8_t cc; std::uint64_t flags; bool expected; };
    constexpr std::uint64_t CF = 1ULL << 0;
    constexpr std::uint64_t PF = 1ULL << 2;
    constexpr std::uint64_t ZF = 1ULL << 6;
    constexpr std::uint64_t SF = 1ULL << 7;
    constexpr std::uint64_t OF = 1ULL << 11;
    const std::array<Case,16> cases = {{
        {0x0, OF, true},              // O
        {0x1, OF, false},             // NO
        {0x2, CF, true},              // B/C/NAE
        {0x3, CF, false},             // AE/NB/NC
        {0x4, ZF, true},              // E/Z
        {0x5, ZF, false},             // NE/NZ
        {0x6, CF|ZF, true},           // BE/NA
        {0x7, CF|ZF, false},          // A/NBE
        {0x8, SF, true},              // S
        {0x9, SF, false},             // NS
        {0xA, PF, true},              // P/PE
        {0xB, PF, false},             // NP/PO
        {0xC, SF, true},              // L/NGE (SF != OF)
        {0xD, SF, false},             // GE/NL
        {0xE, ZF, true},              // LE/NG (ZF || SF != OF)
        {0xF, ZF, false},             // G/NLE
    }};
    for (const auto& test : cases) {
        if (!Run(memory, test.flags, 0xAABBCCDDEEFF0000ULL,
                 {0x0F, static_cast<std::uint8_t>(0x90U + test.cc), 0xC0},
                 result,resultFlags)) return 8;
        const std::uint64_t expected = 0xAABBCCDDEEFF0000ULL | (test.expected ? 1ULL : 0ULL);
        if (result != expected || resultFlags != test.flags) return 9;
    }

    std::cout << "x86 SETcc test: PASS\n";
    return 0;
}
