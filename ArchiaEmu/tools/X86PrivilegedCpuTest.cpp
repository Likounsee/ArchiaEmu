#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
using namespace myps5emu;
static bool Run(Memory& m, Cpu& c, const std::initializer_list<std::uint8_t>& code) {
    std::uint64_t ip=0x1000; std::size_t i=0;
    for(auto b:code) m.Write(ip+i,&b,1),++i;
    c.ConnectMemory(&m); c.SetInstructionPointer(ip); return c.Run()==0;
}
int main() {
    Memory m; m.Map(0x1000,0x1000,MemoryPermission::Read|MemoryPermission::Write|MemoryPermission::Execute);
    Cpu cpu; cpu.SetRflags(0x202);
    if(!Run(m,cpu,{0xFA,0xFB,0xF4})) return 1;
    if((cpu.Rflags()&0x200)==0) return 2;
    Cpu user; user.SetRflags(0x202); user.SetCodeSegment(0x1B);
    bool gp=false; user.SetExceptionHandler([&](Cpu&,const CpuException& e){gp=e.vector==CpuExceptionVector::GeneralProtection; return false;});
    if(Run(m,user,{0xFA})) return 3;
    if(!gp) return 4;
    Cpu userHlt; userHlt.SetCodeSegment(0x1B);
    gp=false; userHlt.SetExceptionHandler([&](Cpu&,const CpuException& e){gp=e.vector==CpuExceptionVector::GeneralProtection; return false;});
    if(Run(m,userHlt,{0xF4})) return 5;
    if(!gp) return 6;
    Cpu cr; cr.SetCodeSegment(0x8); cr.WriteRegister64(0, 0x4000);
    if(!Run(m,cr,{0x0F,0x22,0xD8,0x0F,0x20,0xC0,0xF4})) return 7;
    if(cr.Cr3()!=0x4000 || cr.ReadRegister64(0)!=0x4000) return 8;
    std::cout<<"x86 privileged CPU instruction test: PASS\n";
    return 0;
}
