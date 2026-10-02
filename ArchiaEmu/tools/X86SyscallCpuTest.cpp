#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
using namespace myps5emu;
int main(){
 Memory memory;
 if(!memory.Map(0x1000,0x1000,MemoryPermission::Read|MemoryPermission::Write|MemoryPermission::Execute)) return 1;
 if(!memory.Write(0x1000,std::array<std::uint8_t,5>{0x0f,0x05,0x0f,0x07,0xf4}.data(),4)) return 2;
 Cpu cpu; cpu.ConnectMemory(&memory); cpu.SetInstructionPointer(0x1000); cpu.SetCodeSegment(0x1b); cpu.SetStackSegment(0x23); cpu.SetRflags(0x246);
 cpu.SetMsrStar((0x0000000000000008ULL<<32)|(0x0000000000000010ULL<<48));
 cpu.SetMsrLstar(0x1002); cpu.SetMsrFmask(1ULL<<0);
 bool called=false;
 cpu.SetSyscallHandler([&](Cpu& c){ called=true; if(c.CodeSegment()!=8||c.StackSegment()!=16||c.InstructionPointer()!=0x1002) return false; if(c.ReadRegister64(1)!=0x1002||c.ReadRegister64(11)!=0x246) return false; c.WriteRegister64(1,0x1004); return true; });
 if(cpu.Run()!=0||!called||cpu.InstructionPointer()!=0x1005||cpu.CodeSegment()!=0x18||cpu.StackSegment()!=0x20) return 3;
 // SYSRET is reached only if handler returns to the 0x1002 byte sequence.
 std::cout<<"x86 SYSCALL CPU integration test: PASS\n"; return 0;
}
