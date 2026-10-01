#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;

static bool Run(Cpu& cpu, Memory& memory, std::uint64_t flags, const std::array<std::uint8_t,3>& code)
{
    if (!memory.Write(0x1000, code.data(), code.size())) return false;
    const std::uint8_t hlt = 0xF4;
    if (!memory.Write(0x1003, &hlt, 1)) return false;
    cpu.SetInstructionPointer(0x1000);
    cpu.SetRflags(flags);
    return cpu.Run() == 0;
}

int main()
{
    Memory memory;
    if (!memory.Map(0x1000,0x1000,MemoryPermission::Read|MemoryPermission::Write|MemoryPermission::Execute) ||
        !memory.Map(0x2000,0x1000,MemoryPermission::Read|MemoryPermission::Write)) return 1;
    Cpu cpu;
    cpu.ConnectMemory(&memory);
    cpu.WriteRegister64(0,0x2000);
    // SETE AH: no REX, rm=4 means AH.
    cpu.WriteRegister64(0,0x1234000000000000ULL);
    if (!Run(cpu,memory,1ULL<<6,{0x0F,0x94,0xC4})) return 2;
    if (cpu.ReadRegister64(0) != 0x1234000000000100ULL) return 3;
    // SETP AH: PF=1 must set AH and preserve the rest.
    cpu.WriteRegister64(0,0x1234000000000000ULL);
    if (!Run(cpu,memory,1ULL<<2,{0x0F,0x9A,0xC4})) return 4;
    if (cpu.ReadRegister64(0) != 0x1234000000000100ULL) return 5;
    // SETE byte memory form.
    cpu.WriteRegister64(0,0x2000);
    if (!Run(cpu,memory,1ULL<<6,{0x0F,0x94,0x00})) return 6;
    std::uint8_t value=0;
    if (!memory.Read(0x2000,&value,1) || value != 1) return 7;
    std::cout << "x86 SETcc test: PASS\n";
    return 0;
}
