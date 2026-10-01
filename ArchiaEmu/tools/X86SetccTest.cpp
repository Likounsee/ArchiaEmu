#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;

static bool Run(Memory& memory, std::uint64_t flags, std::uint64_t initialRax,
                 const std::array<std::uint8_t,3>& code, std::uint64_t& resultRax)
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
    return true;
}

int main()
{
    Memory memory;
    if (!memory.Map(0x1000,0x1000,MemoryPermission::Read|MemoryPermission::Write|MemoryPermission::Execute) ||
        !memory.Map(0x2000,0x1000,MemoryPermission::Read|MemoryPermission::Write)) return 1;
    std::uint64_t result = 0;
    // SETE AH: no REX, rm=4 means AH.
    cpu.WriteRegister64(0,0x1234000000000000ULL);
    if (!Run(memory,1ULL<<6,0x1234000000000000ULL,{0x0F,0x94,0xC4},result)) return 2;
    if (result != 0x1234000000000100ULL) return 3;
    // SETP AH: PF=1 must set AH and preserve the rest.
    if (!Run(memory,1ULL<<2,0x1234000000000000ULL,{0x0F,0x9A,0xC4},result)) return 4;
    if (result != 0x1234000000000100ULL) return 5;
    // SETE byte memory form.
    if (!Run(memory,1ULL<<6,0x2000,{0x0F,0x94,0x00},result)) return 6;
    std::uint8_t value=0;
    if (!memory.Read(0x2000,&value,1) || value != 1) return 7;
    std::cout << "x86 SETcc test: PASS\n";
    return 0;
}
