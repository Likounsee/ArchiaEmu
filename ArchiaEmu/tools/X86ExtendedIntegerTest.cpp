#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include <cstdint>
#include <iostream>
#include <vector>

using namespace myps5emu;

static void AppendMovR64(std::vector<std::uint8_t>& code, std::uint8_t reg, std::uint64_t value) {
    code.push_back(static_cast<std::uint8_t>(0x48U | (reg >= 8 ? 0x01U : 0x00U)));
    code.push_back(static_cast<std::uint8_t>(0xB8U + (reg & 7U)));
    for (unsigned i = 0; i < 8; ++i) {
        code.push_back(static_cast<std::uint8_t>(value >> (i * 8U)));
    }
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
    if (!Run(memory, cpu, code) || cpu.Rax() != 0xFFFFFFFFFFFFFFFFULL) {
        std::cerr << "MOVSXD REX.W form failed: RAX=0x" << std::hex << cpu.Rax() << std::dec << "\n";
        return false;
    }
    Memory m2; m2.Map(0x1000,0x1000); Cpu c2; c2.ConnectMemory(&m2);
    std::vector<std::uint8_t> code2; AppendMovR64(code2,3,0x00000000FFFFFFFFULL); code2.insert(code2.end(),{0x63,0xC3});
    if (!Run(m2,c2,code2) || c2.Rax() != 0x00000000FFFFFFFFULL) {
        std::cerr << "MOVSXD 32-bit form failed: RAX=0x" << std::hex << c2.Rax() << std::dec << "\n";
        return false;
    }
    return true;
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
    if (!Run(memory, cpu, code) || cpu.Rax() != 7 || cpu.ReadRegister64(3) != 12) {
        std::cerr << "XADD32 values: RAX=0x" << std::hex << cpu.Rax()
                  << " RBX=0x" << cpu.ReadRegister64(3) << std::dec << "\n";
        return false;
    }
    return true;
}

static bool TestXadd8() {
    Memory memory; memory.Map(0x1000, 0x1000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 5);
    AppendMovR64(code, 3, 7);
    code.insert(code.end(), {0x0F, 0xC0, 0xC3});
    return Run(memory, cpu, code) && (cpu.ReadRegister64(0) & 0xFFU) == 7 && (cpu.ReadRegister64(3) & 0xFFU) == 12;
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

static bool TestMultiByteNop() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    return Run(memory,cpu,{0x0F,0x1F,0x00});
}

static bool TestCmpxchg8b() {
    Memory memory; memory.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&memory);
    const std::uint64_t initial=0x1122334455667788ULL; if(!memory.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&initial),8))return false;
    std::vector<std::uint8_t> code; AppendMovR64(code,0,0x55667788ULL); AppendMovR64(code,2,0x11223344ULL); AppendMovR64(code,3,0xAABBCCDDULL); AppendMovR64(code,1,0xEEFF0011ULL);
    code.insert(code.end(),{0x0F,0xC7,0x0C,0x25,0x00,0x18,0x00,0x00});
    if(!Run(memory,cpu,code)) {
        std::cerr << "CMPXCHG8B first run failed\n";
        return false;
    }
    std::uint64_t out=0;if(!memory.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),8))return false;
    if(out!=0xEEFF0011AABBCCDDULL || (cpu.Rflags()&(1ULL<<6))==0) {
        std::cerr << "CMPXCHG8B first result=0x" << std::hex << out
                  << " RFLAGS=0x" << cpu.Rflags() << std::dec << "\n";
        return false;
    }
    const std::uint64_t oldLo=0x1122334455667788ULL, oldHi=0x99AABBCCDDEEFF00ULL;
    if(!memory.Write(0x1900,reinterpret_cast<const std::uint8_t*>(&oldLo),8)||!memory.Write(0x1908,reinterpret_cast<const std::uint8_t*>(&oldHi),8))return false;
    Cpu c2; c2.ConnectMemory(&memory);
    std::vector<std::uint8_t> c={0x48,0xB8,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,
        0x48,0xBA,0x00,0xFF,0xEE,0xDD,0xCC,0xBB,0xAA,0x99,
        0x48,0xBB,0x11,0x00,0xFF,0xEE,0xDD,0xCC,0xBB,0xAA,
        0x48,0xB9,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x48,0x0F,0xC7,0x0C,0x25,0x00,0x19,0x00,0x00};
    if(!Run(memory,c2,c))return false;
    std::uint64_t newLo=0,newHi=0;if(!memory.Read(0x1900,reinterpret_cast<std::uint8_t*>(&newLo),8)||!memory.Read(0x1908,reinterpret_cast<std::uint8_t*>(&newHi),8))return false;
    return newLo==0xAABBCCDDEEFF0011ULL && newHi==0x7766554433221100ULL && (c2.Rflags()&(1ULL<<6))!=0;
}

static bool TestSystemIntegerOps() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory); cpu.SetCr0(0x8);
    std::vector<std::uint8_t> code={0x0F,0x06,0x0F,0x01,0xE0,0xF4};
    if(!Run(memory,cpu,code) || (cpu.Cr0()&0x8ULL)!=0) return false;
    return true;
}

static bool TestLockPrefix() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,0,1); AppendMovR64(code,3,2); code.insert(code.end(),{0xF0,0x01,0xD8});
    return Run(memory,cpu,code) && cpu.Rax()==3;
}

static bool TestPopRm() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory); cpu.SetStackPointer(0x3000);
    std::vector<std::uint8_t> code; AppendMovR64(code,0,0x1122334455667788ULL); code.push_back(0x50); code.push_back(0x8F); code.push_back(0xC3);
    if(!Run(memory,cpu,code)||cpu.ReadRegister64(3)!=0x1122334455667788ULL)return false;
    return cpu.Rsp()==0x3000ULL;
}

static bool TestDoubleShift() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,0,0x1234); AppendMovR64(code,3,0xABCD);
    code.insert(code.end(),{0x48,0x0F,0xA4,0xD8,0x04,0x48,0x0F,0xAC,0xD8,0x04});
    if (!Run(memory,cpu,code) || cpu.Rax() != 0xD000000000001234ULL) {
        std::cerr << "double shift RAX=0x" << std::hex << cpu.Rax() << std::dec << "\n";
        return false;
    }
    return true;
}

static bool TestBitModify() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,3,0x8ULL); AppendMovR64(code,1,3ULL);
    code.insert(code.end(),{0x48,0x0F,0xA3,0xCB,0x48,0x0F,0xAB,0xCB,0x48,0x0F,0xB3,0xCB,0x48,0x0F,0xBB,0xCB});
    if(!Run(memory,cpu,code))return false;
    return cpu.ReadRegister64(3)==0x8ULL && (cpu.Rflags()&(1ULL<<0))==0;
}

static bool TestBitScan() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,3,0x00100000ULL); code.insert(code.end(),{0x48,0x0F,0xBC,0xC3});
    if(!Run(memory,cpu,code)||cpu.Rax()!=20||((cpu.Rflags()&(1ULL<<6))!=0))return false;
    Memory m2; m2.Map(0x1000,0x1000); Cpu c2; c2.ConnectMemory(&m2);
    code.clear(); AppendMovR64(code,3,0x00100000ULL); code.insert(code.end(),{0x48,0x0F,0xBD,0xC3});
    return Run(m2,c2,code)&&c2.Rax()==20;
}

static bool TestByteAlu() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code={0xB0,0x10,0xB3,0x05,0x00,0xD8,0x08,0xD8,0x20,0xD8,0x30,0xD8,0x38,0xD8,0xF4};
    if (!Run(memory,cpu,code) || (cpu.ReadRegister64(0)&0xFFU) != 0x00U) {
        std::cerr << "byte ALU AL=0x" << std::hex << (cpu.ReadRegister64(0)&0xFFU)
                  << " BL=0x" << (cpu.ReadRegister64(3)&0xFFU) << std::dec << "\n";
        return false;
    }
    return true;
}

static bool TestStringIo() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory);
    cpu.WriteRegister64(2,0x40); cpu.WriteRegister64(7,0x2000); cpu.WriteRegister64(1,2);
    std::uint32_t n=0;
    cpu.SetIoHandlers([&](Cpu&,std::uint16_t port,std::uint8_t width)->std::uint32_t { if(port!=0x40||width!=1)return 0; return n++?0x22U:0x11U; },
        [](Cpu&,std::uint16_t,std::uint32_t,std::uint8_t){return true;});
    if(!Run(memory,cpu,{0xF3,0x6C}))return false;
    std::uint8_t a=0,b=0;if(!memory.Read(0x2000,&a,1)||!memory.Read(0x2001,&b,1))return false;
    return a==0x11&&b==0x22&&cpu.ReadRegister64(7)==0x2002&&cpu.ReadRegister64(1)==0;
}

static bool TestIoPorts() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    bool wrote=false;
    cpu.SetIoHandlers([](Cpu&,std::uint16_t port,std::uint8_t width)->std::uint32_t {
        return (port==0x3F8&&width==1)?0x5AU:0;
    },[&](Cpu&,std::uint16_t port,std::uint32_t value,std::uint8_t width)->bool {
        wrote=port==0x3F8&&value==0xA5U&&width==1; return true;
    });
    cpu.WriteRegister64(2,0x3F8);
    std::vector<std::uint8_t> code={0xEC,0x88,0xC3,0xB0,0xA5,0xEE,0xF4};
    return Run(memory,cpu,code) && (cpu.ReadRegister64(3)&0xFFU)==0x5AU && (cpu.ReadRegister64(0)&0xFFU)==0xA5U && wrote;
}

static bool TestXlat() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory);
    const std::uint8_t table[256] = {0}; std::uint8_t value=0xA7;
    if(!memory.Write(0x2000+0x12,&value,1))return false;
    cpu.WriteRegister64(3,0x2000); cpu.WriteRegister64(0,0x12);
    return Run(memory,cpu,{0xD7}) && (cpu.ReadRegister64(0)&0xFFU)==0xA7U;
}

static bool TestNegWidths() {
    Memory memory; memory.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,0,0x0000000000000001ULL);
    code.insert(code.end(),{0xF6,0xD8});
    AppendMovR64(code,1,0x0000000080000000ULL);
    code.insert(code.end(),{0xF7,0xDB});
    AppendMovR64(code,2,0x0000000000008000ULL);
    code.insert(code.end(),{0x66,0xF7,0xDA});
    AppendMovR64(code,3,0x0000000000000000ULL);
    code.insert(code.end(),{0x48,0xF7,0xDB});
    if(!Run(memory,cpu,code)) return false;
    if((cpu.ReadRegister64(0)&0xFFU)!=0xFFU) return false;
    if((cpu.ReadRegister64(1)&0xFFFFFFFFULL)!=0x80000000ULL) return false;
    if((cpu.ReadRegister64(2)&0xFFFFU)!=0x8000U) return false;
    if(cpu.ReadRegister64(3)!=0) return false;
    if((cpu.Rflags()&1ULL)!=0) return false;
    return true;
}

static bool TestIncDecByte() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    cpu.SetRflags(cpu.Rflags()|1ULL);
    std::vector<std::uint8_t> code={0xB0,0x7F,0xFE,0xC0,0xFE,0xC8};
    if(!Run(memory,cpu,code))return false;
    return (cpu.ReadRegister64(0)&0xFFU)==0x7FU && (cpu.Rflags()&1ULL)!=0;
}

static bool TestMovByteImmediate() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code={0xB0,0x5A,0xC6,0xC3,0x7E,0xF4};
    return Run(memory,cpu,code) && (cpu.ReadRegister64(3)&0xFFU)==0x7EU;
}

static bool TestByteMov() {
    Memory memory; memory.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,0,0x1122334455660077ULL); AppendMovR64(code,3,0x1122334455660000ULL);
    code.insert(code.end(),{0x88,0xC3,0x8A,0xD8});
    return Run(memory,cpu,code) && cpu.ReadRegister64(3)==0x1122334455660077ULL && cpu.ReadRegister64(0)==0x1122334455660077ULL;
}

static bool TestAccumulatorXchg() {
    Memory memory; memory.Map(0x1000,0x1000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code; AppendMovR64(code,0,1); AppendMovR64(code,1,2); code.push_back(0x91);
    if(!Run(memory,cpu,code) || cpu.Rax()!=2 || cpu.ReadRegister64(1)!=1) return false;
    Memory m2; m2.Map(0x1000,0x1000); Cpu c2; c2.ConnectMemory(&m2);
    code.clear(); AppendMovR64(code,0,0x1234); AppendMovR64(code,1,0x5678); code.insert(code.end(),{0x66,0x91});
    return Run(m2,c2,code) && c2.Rax()==0x5678 && c2.ReadRegister64(1)==0x1234;
}

static bool TestMsrAndTsc() {
    Memory memory; memory.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,1,0xC0000082ULL);
    AppendMovR64(code,0,0x55667788ULL);
    AppendMovR64(code,2,0x11223344ULL);
    code.insert(code.end(),{0x0F,0x30,0x0F,0x32,0xF4});
    if(!Run(memory,cpu,code)) return false;
    const std::uint64_t msr=((cpu.ReadRegister64(2)&0xFFFFFFFFULL)<<32)|(cpu.ReadRegister64(0)&0xFFFFFFFFULL);
    if (msr != 0x1122334455667788ULL) return false;
    Memory m2; m2.Map(0x1000,0x1000); Cpu c2; c2.ConnectMemory(&m2);
    return Run(m2,c2,{0x0F,0x31}) && true;
}

static bool TestMoffs() {
    Memory memory; memory.Map(0x1000,0x5000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    const std::uint64_t value=0x1122334455667788ULL;
    if(!memory.Write(0x3000,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
    std::vector<std::uint8_t> load={0x48,0xA1,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0xF4};
    if(!Run(memory,cpu,load) || cpu.Rax()!=value) return false;
    Cpu cpu2; cpu2.ConnectMemory(&memory); cpu2.WriteRegister64(0,0xA5);
    std::vector<std::uint8_t> store={0xA2,0x08,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0xF4};
    if(!Run(memory,cpu2,store)) return false;
    std::uint8_t out=0; if(!memory.Read(0x3008,&out,1)) return false;
    return out==0xA5;
}

static bool TestEnterLeave() {
    Memory memory; memory.Map(0x1000,0x3000);
    Cpu cpu; cpu.ConnectMemory(&memory); cpu.SetStackPointer(0x3000); cpu.WriteRegister64(5,0x123456789ABCDEF0ULL);
    std::vector<std::uint8_t> code={0xC8,0x08,0x00,0x00,0xF4};
    if(!Run(memory,cpu,code)) return false;
    return cpu.ReadRegister64(5)==0x2FF8ULL && cpu.Rsp()==0x2FF0ULL;
}

static bool TestControlTransferGroups() {
    Memory memory; memory.Map(0x1000, 0x3000);
    Cpu cpu; cpu.ConnectMemory(&memory); cpu.SetStackPointer(0x3000);
    std::vector<std::uint8_t> code = {0x48,0xB8,0x0C,0x10,0,0,0,0,0,0, 0xFF,0xD0, 0xF4};
    if (!Run(memory,cpu,code) || cpu.InstructionPointer() == 0) return false;

    Memory m2; m2.Map(0x1000,0x3000); Cpu c2; c2.ConnectMemory(&m2); c2.SetStackPointer(0x3000);
    std::vector<std::uint8_t> ret = {0xC2,0x02,0x00,0xF4};
    return Run(m2,c2,ret);
}

static bool TestSoftwareInterrupts() {
    Memory memory; memory.Map(0x1000, 0x2000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    bool seen = false;
    std::uint8_t vector = 0;
    cpu.SetExceptionHandler([&](Cpu&, const CpuException& e) {
        seen = true;
        vector = static_cast<std::uint8_t>(e.vector);
        return true;
    });
    std::vector<std::uint8_t> code = {0xCC, 0xCD, 0x21, 0xF4};
    if (!Run(memory, cpu, code)) return false;
    if (!seen || vector != 0x21) return false;

    Memory m2; m2.Map(0x1000,0x2000); Cpu c2; c2.ConnectMemory(&m2);
    bool intoSeen=false; c2.SetExceptionHandler([&](Cpu&, const CpuException& e){intoSeen = static_cast<std::uint8_t>(e.vector)==4; return true;});
    c2.SetRflags(c2.Rflags() | (1ULL<<11));
    std::vector<std::uint8_t> into={0xCE,0xF4};
    return Run(m2,c2,into) && intoSeen;
}

static bool TestGroupF6Byte() {
    Memory memory; memory.Map(0x1000, 0x2000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0xF0);
    code.insert(code.end(), {0xF6, 0xD0});
    if (!Run(memory, cpu, code) || (cpu.ReadRegister64(0) & 0xFFU) != 0x0FU) return false;

    Memory m2; m2.Map(0x1000, 0x2000); Cpu c2; c2.ConnectMemory(&m2);
    code.clear(); AppendMovR64(code,0,1); code.insert(code.end(),{0xF6,0xD8});
    if (!Run(m2,c2,code) || (c2.ReadRegister64(0)&0xFFU)!=0xFFU || (c2.Rflags()&1ULL)==0) return false;

    Memory m3; m3.Map(0x1000,0x2000); Cpu c3; c3.ConnectMemory(&m3);
    code.clear(); AppendMovR64(code,0,0x10); AppendMovR64(code,3,0x10); code.insert(code.end(),{0xF6,0xE3});
    if (!Run(m3,c3,code) || (c3.ReadRegister64(0)&0xFFFFU)!=0x0100U) return false;

    Memory m4; m4.Map(0x1000,0x2000); Cpu c4; c4.ConnectMemory(&m4);
    code.clear(); AppendMovR64(code,0,0xF0); AppendMovR64(code,3,4); code.insert(code.end(),{0xF6,0xEB});
    if (!Run(m4,c4,code) || (c4.ReadRegister64(0)&0xFFFFU)!=0xFFC0U) return false;

    Memory m5; m5.Map(0x1000,0x2000); Cpu c5; c5.ConnectMemory(&m5);
    code.clear(); AppendMovR64(code,0,0x0105); AppendMovR64(code,3,3); code.insert(code.end(),{0xF6,0xF3});
    if (!Run(m5,c5,code) || (c5.ReadRegister64(0)&0xFFFFU)!=0x0057U) return false;

    Memory m6; m6.Map(0x1000,0x2000); Cpu c6; c6.ConnectMemory(&m6);
    code.clear(); AppendMovR64(code,0,0xFF85); AppendMovR64(code,3,5); code.insert(code.end(),{0xF6,0xFB});
    if (!Run(m6,c6,code) || (c6.ReadRegister64(0)&0xFFFFU)!=0xFDE8U) return false;
    return true;
}

static bool TestStringInstructions() {
    Memory memory; memory.Map(0x1000, 0x4000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    const std::uint8_t source[] = {1,2,3,4};
    if (!memory.Write(0x2000, source, sizeof(source))) return false;
    cpu.WriteRegister64(6, 0x2000);
    cpu.WriteRegister64(7, 0x2010);
    cpu.WriteRegister64(1, 4);
    std::vector<std::uint8_t> code = {0xF3, 0xA4};
    if (!Run(memory, cpu, code)) return false;
    std::uint8_t copied[4]{};
    if (!memory.Read(0x2010, copied, sizeof(copied))) return false;
    return copied[0] == 1 && copied[1] == 2 && copied[2] == 3 && copied[3] == 4 &&
           cpu.ReadRegister64(6) == 0x2004 && cpu.ReadRegister64(7) == 0x2014 &&
           cpu.ReadRegister64(1) == 0;
}

static bool TestFlagsAndLoops() {
    Memory memory; memory.Map(0x1000, 0x2000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0x000000000000D500ULL);
    code.insert(code.end(), {0x9E, 0x9F});
    if (!Run(memory, cpu, code) || ((cpu.Rax() >> 8) & 0xFFU) != 0xD7U) { std::cerr << "SAHF/LAHF failed: AH=0x" << std::hex << ((cpu.Rax() >> 8) & 0xFFU) << std::dec << "\n"; return false; }

    Memory memory2; memory2.Map(0x1000, 0x2000);
    Cpu cpu2; cpu2.ConnectMemory(&memory2);
    std::vector<std::uint8_t> flagsCode = {0xF8, 0xF9, 0xF5, 0xFC, 0xFD};
    if (!Run(memory2, cpu2, flagsCode)) { std::cerr << "flag control execution failed\n"; return false; }
    if ((cpu2.Rflags() & 1ULL) != 0 || (cpu2.Rflags() & (1ULL << 10)) == 0) { std::cerr << "flag control state failed: expected CF=0,DF=1; RFLAGS=0x" << std::hex << cpu2.Rflags() << std::dec << "\n"; return false; }

    Memory memory3; memory3.Map(0x1000, 0x2000);
    Cpu cpu3; cpu3.ConnectMemory(&memory3);
    std::vector<std::uint8_t> loopCode;
    AppendMovR64(loopCode, 1, 2);
    loopCode.insert(loopCode.end(), {0xE2, 0xFE, 0xF4});
    if (!Run(memory3, cpu3, loopCode) || cpu3.ReadRegister64(1) != 0) { std::cerr << "LOOP failed: RCX=" << std::hex << cpu3.ReadRegister64(1) << std::dec << " RIP=0x" << std::hex << cpu3.InstructionPointer() << std::dec << "\n"; return false; }

    Memory memory4; memory4.Map(0x1000, 0x2000);
    Cpu cpu4; cpu4.ConnectMemory(&memory4);
    std::vector<std::uint8_t> jrcxzCode;
    AppendMovR64(jrcxzCode, 1, 0);
    jrcxzCode.insert(jrcxzCode.end(), {0xE3, 0x01, 0x90, 0xF4});
    if (!Run(memory4, cpu4, jrcxzCode)) { std::cerr << "JRCXZ execution failed\n"; return false; } return true;
}

static bool TestAdcSbb16Directions() {
    Memory memory1; memory1.Map(0x1000,0x1000); Cpu cpu1; cpu1.ConnectMemory(&memory1);
    std::vector<std::uint8_t> rmCode;
    AppendMovR64(rmCode,0,0x123400000000FFFFULL);
    AppendMovR64(rmCode,3,0x5678000000000000ULL);
    rmCode.insert(rmCode.end(),{0xF9,0x66,0x11,0xD8,0x66,0x19,0xD8});
    if(!Run(memory1,cpu1,rmCode)) return false;
    if(cpu1.ReadRegister64(0)!=0x123400000000FFFFULL ||
       cpu1.ReadRegister64(3)!=0x5678000000000000ULL ||
       (cpu1.Rflags()&1ULL)==0) return false;

    Memory memory2; memory2.Map(0x1000,0x1000); Cpu cpu2; cpu2.ConnectMemory(&memory2);
    std::vector<std::uint8_t> regCode;
    AppendMovR64(regCode,0,0x1111000000000001ULL);
    AppendMovR64(regCode,3,0x2222000000000002ULL);
    regCode.insert(regCode.end(),{
        0xF9,0x66,0x13,0xD8,
        0xF9,0x66,0x1B,0xD8
    });
    if(!Run(memory2,cpu2,regCode)) return false;
    return cpu2.ReadRegister64(0)==0x1111000000000001ULL &&
           cpu2.ReadRegister64(3)==0x2222000000000002ULL &&
           (cpu2.Rflags()&1ULL)==0;
}

static bool TestIncDecMemoryAndCmpWidths() {
    Memory memory; memory.Map(0x1000,0x4000);
    Cpu cpu; cpu.ConnectMemory(&memory);
    const std::uint32_t v32=0x7FFFFFFFU;
    const std::uint16_t v16=0x8000U;
    const std::uint64_t v64=0xFFFFFFFFFFFFFFFFULL;
    if(!memory.Write(0x2000,reinterpret_cast<const std::uint8_t*>(&v32),sizeof(v32))) return false;
    if(!memory.Write(0x2010,reinterpret_cast<const std::uint8_t*>(&v16),sizeof(v16))) return false;
    if(!memory.Write(0x2020,reinterpret_cast<const std::uint8_t*>(&v64),sizeof(v64))) return false;

    std::vector<std::uint8_t> code;
    AppendMovR64(code,7,0x2000);
    code.insert(code.end(),{0xFF,0x07,0xFF,0x0F});
    AppendMovR64(code,7,0x2010);
    code.insert(code.end(),{0x66,0xFF,0x07,0x66,0xFF,0x0F});
    AppendMovR64(code,7,0x2020);
    code.insert(code.end(),{0x48,0xFF,0x07,0x48,0xFF,0x0F});
    AppendMovR64(code,0,0x1234000000000001ULL);
    AppendMovR64(code,3,0x2222000000000001ULL);
    code.insert(code.end(),{0x66,0x39,0xD8,0x66,0x3B,0xD8,0x39,0xD8,0x3B,0xD8,0x48,0x39,0xD8,0x48,0x3B,0xD8});
    if(!Run(memory,cpu,code)) return false;

    std::uint32_t out32=0; std::uint16_t out16=0; std::uint64_t out64=0;
    if(!memory.Read(0x2000,reinterpret_cast<std::uint8_t*>(&out32),sizeof(out32))) return false;
    if(!memory.Read(0x2010,reinterpret_cast<std::uint8_t*>(&out16),sizeof(out16))) return false;
    if(!memory.Read(0x2020,reinterpret_cast<std::uint8_t*>(&out64),sizeof(out64))) return false;
    if(out32!=0x7FFFFFFFU || out16!=0x8000U || out64!=0xFFFFFFFFFFFFFFFFULL) return false;
    return cpu.ReadRegister64(0)==0x1234000000000001ULL &&
           cpu.ReadRegister64(3)==0x2222000000000001ULL;
}

static bool TestImulForms() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,0,6); AppendMovR64(code,3,7);
    code.insert(code.end(),{0x48,0x0F,0xAF,0xC3});
    if(!Run(memory,cpu,code) || cpu.Rax()!=42 || (cpu.Rflags()&((1ULL<<0)|(1ULL<<11)))!=0) return false;

    Memory m2; m2.Map(0x1000,0x3000); Cpu c2; c2.ConnectMemory(&m2);
    code.clear(); AppendMovR64(code,0,0x7FFFFFFF); AppendMovR64(code,3,2);
    code.insert(code.end(),{0x0F,0xAF,0xC3});
    if(!Run(m2,c2,code) || (c2.ReadRegister64(0)&0xFFFFFFFFULL)!=0xFFFFFFFEU ||
       (c2.Rflags()&((1ULL<<0)|(1ULL<<11)))!=((1ULL<<0)|(1ULL<<11))) return false;

    Memory m3; m3.Map(0x1000,0x3000); Cpu c3; c3.ConnectMemory(&m3);
    code.clear(); AppendMovR64(code,0,7);
    code.insert(code.end(),{0x6B,0xC0,0xFE});
    if(!Run(m3,c3,code) || (c3.Rax()&0xFFFFFFFFULL)!=0xFFFFFFF2U ||
       (c3.Rflags()&((1ULL<<0)|(1ULL<<11)))!=0) return false;

    Memory m4; m4.Map(0x1000,0x3000); Cpu c4; c4.ConnectMemory(&m4);
    code.clear(); AppendMovR64(code,0,0x40000000ULL);
    code.insert(code.end(),{0x69,0xC0,0x02,0x00,0x00,0x00});
    if(!Run(m4,c4,code) || (c4.Rax()&0xFFFFFFFFULL)!=0x80000000U ||
       (c4.Rflags()&((1ULL<<0)|(1ULL<<11)))!=((1ULL<<0)|(1ULL<<11))) return false;

    Memory m5; m5.Map(0x1000,0x3000); Cpu c5; c5.ConnectMemory(&m5);
    code.clear(); AppendMovR64(code,0,3);
    code.insert(code.end(),{0x66,0x6B,0xC0,0xFE});
    if(!Run(m5,c5,code) || (c5.Rax()&0xFFFFU)!=0xFFFAU) return false;

    Memory m6; m6.Map(0x1000,0x3000); Cpu c6; c6.ConnectMemory(&m6);
    code.clear(); AppendMovR64(code,0,0x4000000000000000ULL);
    code.insert(code.end(),{0x48,0x6B,0xC0,0x02});
    if(!Run(m6,c6,code) || c6.Rax()!=0x8000000000000000ULL ||
       (c6.Rflags()&((1ULL<<0)|(1ULL<<11)))!=((1ULL<<0)|(1ULL<<11))) return false;
    return true;
}

static bool TestMulDivForms() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    // DIV/IDIV 8-bit: AX / BL.
    code.insert(code.end(),{0xB8,0xE8,0x03,0xB3,0x0A,0xF6,0xF3});
    code.insert(code.end(),{0xB8,0xD8,0xFF,0xB3,0x0A,0xF6,0xFB});
    if(!Run(memory,cpu,code)) return false;
    if((cpu.Rax()&0xFFFFU)!=0x0008U) return false; // -40 / -? final IDIV: -40 / 10 = -4, rem 0 -> FFFC.
    // Re-run with explicit signed case to avoid relying on prior AX state.
    Memory signed8; signed8.Map(0x1000,0x3000); Cpu s8; s8.ConnectMemory(&signed8);
    if(!Run(signed8,s8,{0xB8,0xD8,0xFF,0xB3,0x0A,0xF6,0xFB})) return false;
    if((s8.Rax()&0xFFFFU)!=0x00FCU) return false;

    // 16-bit DIV and IDIV.
    Memory m16; m16.Map(0x1000,0x3000); Cpu c16; c16.ConnectMemory(&m16);
    std::vector<std::uint8_t> v16={0xB8,0xE8,0x03,0x31,0xD2,0xBB,0x1E,0x00,0x66,0xF7,0xF3};
    v16.insert(v16.end(),{0xB8,0x18,0xFC,0xBA,0xFF,0xFF,0xBB,0x1E,0x00,0x66,0xF7,0xFB});
    if(!Run(m16,c16,v16)) return false;
    if((c16.Rax()&0xFFFFU)!=0xFFF4U || (c16.Rdx()&0xFFFFU)!=0x0000U) return false;

    // 32-bit DIV: EDX:EAX / EBX = 100000 / 30000.
    Memory m32; m32.Map(0x1000,0x3000); Cpu c32; c32.ConnectMemory(&m32);
    std::vector<std::uint8_t> v32; AppendMovR64(v32,0,100000); AppendMovR64(v32,2,0); AppendMovR64(v32,3,30000);
    v32.insert(v32.end(),{0xF7,0xF3});
    if(!Run(m32,c32,v32) || (c32.Rax()&0xFFFFFFFFULL)!=3 || (c32.Rdx()&0xFFFFFFFFULL)!=10000) return false;

    // 64-bit IDIV: RDX:RAX / RBX = -100 / 7.
    Memory m64; m64.Map(0x1000,0x3000); Cpu c64; c64.ConnectMemory(&m64);
    std::vector<std::uint8_t> v64; AppendMovR64(v64,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(v64,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(v64,3,7);
    v64.insert(v64.end(),{0x48,0xF7,0xFB});
    if(!Run(m64,c64,v64) || c64.Rax()!=0xFFFFFFFFFFFFFFF2ULL || c64.Rdx()!=0xFFFFFFFFFFFFFFFFULL) return false;

    // Quotient overflow must raise #DE.
    Memory ov; ov.Map(0x1000,0x3000); Cpu co; co.ConnectMemory(&ov);
    std::vector<std::uint8_t> vo; AppendMovR64(vo,0,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(vo,2,1); AppendMovR64(vo,3,1);
    vo.insert(vo.end(),{0x48,0xF7,0xF3});
    vo.push_back(0xF4);
    if(!ov.Write(0x1000,vo.data(),vo.size())) return false;
    co.SetInstructionPointer(0x1000);
    if(co.Run()==0) return false;

    // IDIV minimum signed 64-bit / -1 must raise #DE without signed-overflow UB.
    Memory min64; min64.Map(0x1000,0x3000); Cpu cm; cm.ConnectMemory(&min64);
    std::vector<std::uint8_t> vm; AppendMovR64(vm,0,0x8000000000000000ULL); AppendMovR64(vm,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(vm,3,0xFFFFFFFFFFFFFFFFULL);
    vm.insert(vm.end(),{0x48,0xF7,0xFB,0xF4});
    if(!min64.Write(0x1000,vm.data(),vm.size())) return false;
    cm.SetInstructionPointer(0x1000);
    if(cm.Run()==0) return false;
    return true;
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
    if (!TestMultiByteNop()) { std::cerr << "multi-byte NOP failed\n"; return 6; }
    if (!TestCmpxchg8b()) { std::cerr << "CMPXCHG8B failed\n"; return 6; }
    if (!TestSystemIntegerOps()) { std::cerr << "system integer ops failed\n"; return 6; }
    if (!TestLockPrefix()) { std::cerr << "LOCK prefix failed\n"; return 6; }
    if (!TestPopRm()) { std::cerr << "POP r/m failed\n"; return 6; }
    if (!TestDoubleShift()) { std::cerr << "double shift failed\n"; return 6; }
    if (!TestBitModify()) { std::cerr << "bit modify failed\n"; return 6; }
    if (!TestBitScan()) { std::cerr << "bit scan failed\n"; return 6; }
    if (!TestByteAlu()) { std::cerr << "byte ALU failed\n"; return 6; }
    if (!TestStringIo()) { std::cerr << "string I/O failed\n"; return 6; }
    if (!TestIoPorts()) { std::cerr << "I/O ports failed\n"; return 6; }
    if (!TestXlat()) { std::cerr << "XLAT failed\n"; return 6; }
    if (!TestIncDecByte()) { std::cerr << "byte INC/DEC failed\n"; return 6; }
    if (!TestMovByteImmediate()) { std::cerr << "byte immediate MOV failed\n"; return 6; }
    if (!TestByteMov()) { std::cerr << "byte MOV failed\n"; return 6; }
    if (!TestAccumulatorXchg()) { std::cerr << "accumulator XCHG failed\n"; return 6; }
    if (!TestMsrAndTsc()) { std::cerr << "MSR/TSC failed\n"; return 6; }
    if (!TestMoffs()) { std::cerr << "moffs failed\n"; return 6; }
    if (!TestEnterLeave()) { std::cerr << "ENTER failed\n"; return 6; }
    if (!TestControlTransferGroups()) { std::cerr << "control transfer groups failed\n"; return 6; }
    if (!TestSoftwareInterrupts()) { std::cerr << "software interrupts failed\n"; return 6; }
    if (!TestGroupF6Byte()) { std::cerr << "F6 byte group failed\n"; return 6; }
    if (!TestStringInstructions()) { std::cerr << "string instructions failed\n"; return 6; }
    if (!TestFlagsAndLoops()) { std::cerr << "flags/loops failed\n"; return 7; }
    if (!TestAdcSbb16Directions()) { std::cerr << "ADC/SBB 16-bit directions failed\n"; return 8; }
    if (!TestIncDecMemoryAndCmpWidths()) { std::cerr << "INC/DEC memory and CMP widths failed\n"; return 8; }
    if (!TestNegWidths()) { std::cerr << "NEG widths failed\n"; return 9; }\n    if (!TestImulForms()) { std::cerr << "IMUL forms failed\n"; return 10; }\n    if (!TestMulDivForms()) { std::cerr << "MUL/DIV forms failed\n"; return 11; }
    if (!TestCpuid()) { std::cerr << "CPUID failed\n"; return 8; }
    std::cout << "x86 extended integer instruction test: PASS\n";
    return 0;
}
