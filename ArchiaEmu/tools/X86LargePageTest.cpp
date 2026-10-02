#include "cpu/x86/Paging.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
using namespace myps5emu;
int main(){
 Memory m;
 if(!m.Map(0x1000,0x5000,MemoryPermission::Read|MemoryPermission::Write)) return 1;
 auto q=[&](std::uint64_t a,std::uint64_t v){return m.Write(a,reinterpret_cast<const std::uint8_t*>(&v),8);};
 q(0x1000,0x2000|7); q(0x2000,0x3000|7); q(0x3000,0x800000|0x87);
 Paging p(m); p.SetCr3(0x1000); p.SetCr4(1ULL<<5); p.SetEfer(1ULL<<11);
 auto r=p.Translate(0x12345,false,false,false);
 std::cout<<"ok="<<r.ok<<" phys=0x"<<std::hex<<r.physical_address<<" fault="<<std::dec<<static_cast<unsigned>(r.fault)<<" err=0x"<<std::hex<<r.page_fault_error<<"\n";
 return (r.ok && r.physical_address==0x812345)?0:1;
}
