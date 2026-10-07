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

static bool TestCmovccExtendedConditions() {
    // CF condition with REX.R/B: CMOVC R8, R9.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0x11);
        AppendMovR64(code,9,0x2233445566778899ULL);
        code.push_back(0xF9); // STC
        code.insert(code.end(),{0x4D,0x0F,0x42,0xC1});
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=0x2233445566778899ULL) return false;
    }

    // ZF condition must not move when the compare is unequal.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0x11);
        AppendMovR64(code,9,0x22);
        code.insert(code.end(),{0x4C,0x39,0xC8}); // CMP RAX, R9
        code.insert(code.end(),{0x4D,0x0F,0x44,0xC1}); // CMOVZ R8,R9
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=0x11ULL) return false;
    }

    // OF/SF conditions from a signed overflow boundary.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0x55);
        AppendMovR64(code,9,0x66);
        AppendMovR64(code,0,0x80000000ULL);
        AppendMovR64(code,3,1);
        code.insert(code.end(),{0x39,0xD8}); // CMP EAX, EBX: INT_MIN - 1
        code.insert(code.end(),{0x45,0x0F,0x40,0xC1}); // CMOVO R8D,R9D (taken)
        code.insert(code.end(),{0x45,0x0F,0x48,0xC1}); // CMOVS R8D,R9D (not taken)
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=0x66ULL) return false;
    }

    // PF condition with an extended memory source.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x1122334455667788ULL;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0);
        AppendMovR64(code,0,0);
        code.insert(code.end(),{0x48,0x31,0xC0}); // XOR RAX,RAX -> PF=1
        AppendMovR64(code,11,0x1800);
        code.insert(code.end(),{0x4D,0x0F,0x4A,0x43,0x08}); // CMOVP R8,[R11+8]
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=value) return false;
    }

    return true;
}

static bool TestCmov16ExtendedMemoryAndFlags() {
    constexpr std::uint64_t CF = 1ULL;
    constexpr std::uint64_t PF = 1ULL << 2;
    constexpr std::uint64_t ZF = 1ULL << 6;
    constexpr std::uint64_t OF = 1ULL << 11;

    Memory m; m.Map(0x1000, 0x3000); Cpu cpu; cpu.ConnectMemory(&m);
    const std::uint16_t source = 0xBEEF;
    if (!m.Write(0x1808, reinterpret_cast<const std::uint8_t*>(&source), sizeof(source))) return false;

    std::vector<std::uint8_t> code;
    AppendMovR64(code, 8, 0x1234000000000011ULL);
    AppendMovR64(code, 11, 0x1800);
    cpu.SetRflags(CF | PF | ZF | OF);
    code.insert(code.end(), {0x66, 0x45, 0x0F, 0x44, 0x43, 0x08}); // CMOVZ R8W,[R11+8]

    if (!Run(m, cpu, code)) return false;
    if (cpu.ReadRegister64(8) != 0x123400000000BEEFULL) return false;
    return cpu.Rflags() == (CF | PF | ZF | OF);
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
    if (!Run(memory4, cpu4, jrcxzCode)) { std::cerr << "JRCXZ execution failed\\n"; return false; }

    Memory memory5; memory5.Map(0x1000, 0x2000);
    Cpu cpu5; cpu5.ConnectMemory(&memory5);
    const std::uint64_t preservedFlags = (1ULL << 0) | (1ULL << 2) | (1ULL << 6) | (1ULL << 7) | (1ULL << 11);
    cpu5.SetRflags(preservedFlags);
    std::vector<std::uint8_t> address32Zero;
    AppendMovR64(address32Zero, 1, 0x0000000100000000ULL);
    address32Zero.insert(address32Zero.end(), {0x67, 0xE3, 0x03, 0xB0, 0x01, 0xF4, 0xB0, 0x02, 0xF4});
    if (!Run(memory5, cpu5, address32Zero) || (cpu5.Rax() & 0xFFU) != 0x02U) return false;
    if (cpu5.Rflags() != preservedFlags) return false;

    Memory memory7; memory7.Map(0x1000, 0x2000);
    Cpu cpu7; cpu7.ConnectMemory(&memory7);
    std::vector<std::uint8_t> loop32;
    AppendMovR64(loop32, 1, 0x0000000100000002ULL);
    loop32.insert(loop32.end(), {0x67, 0xE2, 0xFD, 0xF4});
    if (!Run(memory7, cpu7, loop32) || cpu7.ReadRegister64(1) != 0ULL) return false;

    Memory memory8; memory8.Map(0x1000, 0x2000);
    Cpu cpu8; cpu8.ConnectMemory(&memory8);
    cpu8.SetRflags(1ULL << 6);
    std::vector<std::uint8_t> loope32;
    AppendMovR64(loope32, 1, 0x0000000100000002ULL);
    loope32.insert(loope32.end(), {0x67, 0xE1, 0xFD, 0xF4});
    if (!Run(memory8, cpu8, loope32) || cpu8.ReadRegister64(1) != 0ULL) return false;
    if (cpu8.Rflags() != (1ULL << 6)) return false;

    Memory memory9; memory9.Map(0x1000, 0x2000);
    Cpu cpu9; cpu9.ConnectMemory(&memory9);
    std::vector<std::uint8_t> loopne32;
    AppendMovR64(loopne32, 1, 0x0000000100000002ULL);
    loopne32.insert(loopne32.end(), {0x67, 0xE0, 0xFD, 0xF4});
    if (!Run(memory9, cpu9, loopne32) || cpu9.ReadRegister64(1) != 0ULL) return false;

    Memory memory6; memory6.Map(0x1000, 0x2000);
    Cpu cpu6; cpu6.ConnectMemory(&memory6);
    std::vector<std::uint8_t> address32NonZero;
    AppendMovR64(address32NonZero, 1, 0x0000000100000001ULL);
    address32NonZero.insert(address32NonZero.end(), {0x67, 0xE3, 0x03, 0xB0, 0x01, 0xF4, 0xB0, 0x02, 0xF4});
    if (!Run(memory6, cpu6, address32NonZero) || (cpu6.Rax() & 0xFFU) != 0x01U) return false;

    return true;
}

static bool TestImulImmediateMemoryForms() {
    // IMUL r64, r/m64, imm32 sign-extension from memory.
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=3; if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,8,0x1800);
        code.insert(code.end(),{0x4C,0x69,0x4C,0x24,0x00,0xFE,0xFF,0xFF,0xFF}); // IMUL R9,[RSP],-2
        if(!Run(m,cpu,code) || cpu.ReadRegister64(9)!=static_cast<std::uint64_t>(-6)) return false;
    }
    // IMUL r32, r/m32, imm8 with REX.B memory addressing.
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t value=7; if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),4)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,10,0x1800);
        code.insert(code.end(),{0x45,0x6B,0x4A,0x00,0xFE}); // IMUL R9D,[R10],-2
        if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.ReadRegister64(9))!=static_cast<std::uint32_t>(-14)) return false;
    }
    // 64-bit DIV memory with REX.B addressing.
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t d=7; if(!m.Write(0x1900,reinterpret_cast<const std::uint8_t*>(&d),8)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,0,100); AppendMovR64(code,2,0); AppendMovR64(code,10,0x1900);
        code.insert(code.end(),{0x49,0xF7,0xF2}); // DIV R10
        if(!Run(m,cpu,code) || cpu.Rax()!=14U || cpu.ReadRegister64(2)!=2U) return false;
    }

    return true;
}

static bool TestImulImmediateExtendedRegisters() {
    // IMUL r16, r/m16, imm8 with REX.R/B.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,8,7); AppendMovR64(code,9,6);
        code.insert(code.end(),{0x66,0x45,0x6B,0xC1,0xFE}); // IMUL R8W,R9W,-2
        if(!Run(m,cpu,code) || (cpu.ReadRegister64(8)&0xFFFFU)!=static_cast<std::uint64_t>(static_cast<std::uint16_t>(-12))) return false;
    }
    // IMUL r32, r/m32, imm8 with REX.R/B and sign-extended immediate.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,10,7); AppendMovR64(code,11,6);
        code.insert(code.end(),{0x45,0x6B,0xD3,0xFE}); // IMUL R10D,R11D,-2
        if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.ReadRegister64(10))!=static_cast<std::uint32_t>(-12)) return false;
    }
    return true;
}

static bool TestGroup1RexExtendedRegisters() {
    // Group-1 immediate forms must honor REX.B for R8..R15 and all operand sizes.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,8,0xFF);
        code.insert(code.end(),{0x41,0x80,0xC0,0x01}); // ADD R8B,1
        if(!Run(m,cpu,code) || (cpu.ReadRegister64(8)&0xFFU)!=0U) { std::cerr << "G1 REX byte R8=0x" << std::hex << cpu.ReadRegister64(8) << " RFLAGS=0x" << cpu.Rflags() << "\n"; return false; }
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,9,0xFFFF);
        code.insert(code.end(),{0x66,0x41,0x83,0xC1,0x01}); // ADD R9W,1
        if(!Run(m,cpu,code) || (cpu.ReadRegister64(9)&0xFFFFU)!=0U) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,10,0xFFFFFFFFU);
        code.insert(code.end(),{0x41,0x83,0xC2,0x01}); // ADD R10D,1
        if(!Run(m,cpu,code) || cpu.ReadRegister64(10)!=0U) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,11,0);
        code.insert(code.end(),{0x49,0x83,0xC3,0xFF}); // ADD R11,-1 (sign-extended imm8)
        if(!Run(m,cpu,code) || cpu.ReadRegister64(11)!=0xFFFFFFFFFFFFFFFFULL) return false;
    }
    // REX.R + REX.B register-to-register form: ADD R8D,R9D.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,8,5); AppendMovR64(code,9,7);
        code.insert(code.end(),{0x45,0x01,0xC8});
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=12U || cpu.ReadRegister64(9)!=7U) return false;
    }
    return true;
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

static bool TestAdcSbbImmediateAndWidths() {
    // Register forms across byte/word/dword/qword, both ModRM directions.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0xFF);
        AppendMovR64(code, 3, 0x00);
        code.insert(code.end(), {0xF9, 0x10, 0xD8, 0x18, 0xD8});
        if (!Run(m, c, code) || (c.ReadRegister64(0) & 0xFFU) != 0xFFU ||
            (c.Rflags() & 1ULL) == 0) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0xFFFF);
        AppendMovR64(code, 3, 1);
        code.insert(code.end(), {0xF9, 0x66, 0x13, 0xC3, 0x66, 0x1B, 0xC3});
        if (!Run(m, c, code) || (c.ReadRegister64(0) & 0xFFFFU) != 0xFFFFU ||
            (c.ReadRegister64(3) & 0xFFFFU) != 0x00000001U) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0xFFFFFFFFULL);
        AppendMovR64(code, 3, 1);
        code.insert(code.end(), {0xF9, 0x11, 0xD8, 0x1B, 0xD8});
        if (!Run(m, c, code) || (c.ReadRegister64(0) & 0xFFFFFFFFULL) != 0x00000001ULL ||
            (c.ReadRegister64(3) & 0xFFFFFFFFULL) != 0xFFFFFFFFULL) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code, 3, 1);
        code.insert(code.end(), {0xF9, 0x48, 0x11, 0xD8, 0x48, 0x1B, 0xD8});
        if (!Run(m, c, code) || c.ReadRegister64(0) != 0x0000000000000001ULL ||
            c.ReadRegister64(3) != 0xFFFFFFFFFFFFFFFFULL) return false;
    }

    // Memory ModRM forms across widths.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu c; c.ConnectMemory(&m);
        std::uint8_t v=0x10; if(!m.Write(0x1800,&v,1)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800); AppendMovR64(code,0,1);
        code.insert(code.end(),{0x10,0x07,0x18,0x07});
        if(!Run(m,c,code)) return false;
        if(!m.Read(0x1800,&v,1) || v!=0x10) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu c; c.ConnectMemory(&m);
        std::uint16_t v=0x0010; if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&v),2)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800); AppendMovR64(code,0,1);
        code.insert(code.end(),{0x66,0x11,0x07,0x66,0x19,0x07});
        if(!Run(m,c,code)) return false;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&v),2) || v!=0x0010) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu c; c.ConnectMemory(&m);
        std::uint32_t v=0x00000010U; if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&v),4)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800); AppendMovR64(code,0,1);
        code.insert(code.end(),{0x11,0x07,0x19,0x07});
        if(!Run(m,c,code)) return false;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&v),4) || v!=0x00000010U) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu c; c.ConnectMemory(&m);
        std::uint64_t v=0x0000000000000010ULL; if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&v),8)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800); AppendMovR64(code,0,1);
        code.insert(code.end(),{0x48,0x11,0x07,0x48,0x19,0x07});
        if(!Run(m,c,code)) return false;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&v),8) || v!=0x0000000000000010ULL) return false;
    }

    // Accumulator immediate forms: ADC/SBB byte, word, dword and sign-extended qword.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        if (!Run(m, c, {0xB0, 0xFF, 0xF9, 0x14, 0x00}) ||
            (c.ReadRegister64(0) & 0xFFU) != 0x00U || (c.Rflags() & 1ULL) == 0) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0xFFFF);
        code.insert(code.end(), {0xF9, 0x66, 0x15, 0x00, 0x00});
        if (!Run(m, c, code) ||
            (c.ReadRegister64(0) & 0xFFFFU) != 0x0000U || (c.Rflags() & 1ULL) == 0) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        if (!Run(m, c, {0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0x15, 0x00, 0x00, 0x00, 0x00}) ||
            (c.ReadRegister64(0) & 0xFFFFFFFFULL) != 0x00000000ULL || (c.Rflags() & 1ULL) == 0) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        if (!Run(m, c, {0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                         0xF9, 0x15, 0x00, 0x00, 0x00, 0x00}) ||
            c.ReadRegister64(0) != 0x0000000000000000ULL || (c.Rflags() & 1ULL) == 0) return false;
    }
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu c; c.ConnectMemory(&m);
        if (!Run(m, c, {0xB0, 0x00, 0x1C, 0x01}) ||
            (c.ReadRegister64(0) & 0xFFU) != 0xFFU || (c.Rflags() & 1ULL) == 0) return false;
    }
    return true;
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

    Memory signed64; signed64.Map(0x1000,0x3000); Cpu signedCpu; signedCpu.ConnectMemory(&signed64);
    code.clear(); AppendMovR64(code,0,0xFFFFFFFFFFFFFFFDULL); AppendMovR64(code,3,7);
    code.insert(code.end(),{0x48,0x0F,0xAF,0xC3});
    if(!Run(signed64,signedCpu,code) || signedCpu.Rax()!=0xFFFFFFFFFFFFFFEBULL ||
       (signedCpu.Rflags()&((1ULL<<0)|(1ULL<<11)))!=0) return false;

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

    Memory imm16; imm16.Map(0x1000,0x3000); Cpu imm16Cpu; imm16Cpu.ConnectMemory(&imm16);
    code.clear(); AppendMovR64(code,0,0xFFFDULL);
    code.insert(code.end(),{0x66,0x69,0xC0,0x07,0x00});
    if(!Run(imm16,imm16Cpu,code) || (imm16Cpu.Rax()&0xFFFFU)!=0xFFEBU ||
       (imm16Cpu.Rflags()&((1ULL<<0)|(1ULL<<11)))!=0) return false;

    Memory m6; m6.Map(0x1000,0x3000); Cpu c6; c6.ConnectMemory(&m6);
    code.clear(); AppendMovR64(code,0,0x4000000000000000ULL);
    code.insert(code.end(),{0x48,0x6B,0xC0,0x02});
    if(!Run(m6,c6,code) || c6.Rax()!=0x8000000000000000ULL ||
       (c6.Rflags()&((1ULL<<0)|(1ULL<<11)))!=((1ULL<<0)|(1ULL<<11))) return false;

    // 64-bit IMUL r64,r/m64,imm32 with REX.R/B and memory source.
    Memory m7; m7.Map(0x1000,0x4000); Cpu c7; c7.ConnectMemory(&m7);
    const std::int64_t source = -3;
    if(!m7.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&source),sizeof(source))) return false;
    code.clear();
    AppendMovR64(code,8,0);
    AppendMovR64(code,11,0x1800);
    code.insert(code.end(),{0x4D,0x69,0x43,0x08,0x07,0x00,0x00,0x00}); // IMUL R8,[R11+8],7
    if(!Run(m7,c7,code) || c7.ReadRegister64(8)!=0xFFFFFFFFFFFFFFEBULL) return false;
    if((c7.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    // 64-bit IMUL r64,r/m64,imm8 with REX.R/B and memory source.
    Memory m8; m8.Map(0x1000,0x4000); Cpu c8; c8.ConnectMemory(&m8);
    const std::int64_t source8 = 0x1000000000000000LL;
    if(!m8.Write(0x1810,reinterpret_cast<const std::uint8_t*>(&source8),sizeof(source8))) return false;
    code.clear();
    AppendMovR64(code,9,0);
    AppendMovR64(code,12,0x1800);
    AppendMovR64(code,13,0);
    code.insert(code.end(),{0x4F,0x6B,0x4C,0xAC,0x10,0x08}); // IMUL R9,[R12+R13*4+16],8
    if(!Run(m8,c8,code) || c8.ReadRegister64(9)!=0x8000000000000000ULL) return false;
    if((c8.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    return true;
}

static bool TestMulDivForms() {
    Memory memory; memory.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&memory);
    std::vector<std::uint8_t> code;
    // DIV/IDIV 8-bit: AX / BL.
    {
        std::vector<std::uint8_t> v8;
        AppendMovR64(v8, 0, 0x03E8ULL);
        AppendMovR64(v8, 3, 0x0AULL);
        v8.insert(v8.end(), {0xF6, 0xF3});
        if (!Run(memory, cpu, v8) || (cpu.Rax() & 0xFFFFU) != 0x0064U) return false;
    }
    {
        Memory signed8; signed8.Map(0x1000,0x3000); Cpu s8; s8.ConnectMemory(&signed8);
        std::vector<std::uint8_t> v8;
        AppendMovR64(v8, 0, 0xFFD8ULL);
        AppendMovR64(v8, 3, 0x0AULL);
        v8.insert(v8.end(), {0xF6, 0xFB});
        if (!Run(signed8, s8, v8) || (s8.Rax() & 0xFFFFU) != 0x00FCU) return false;
    }

    // 16-bit DIV and IDIV.
    Memory m16; m16.Map(0x1000,0x3000); Cpu c16; c16.ConnectMemory(&m16);
    std::vector<std::uint8_t> v16;
    v16.insert(v16.end(),{0xB8,0xE8,0x03,0x31,0xD2,0xBB,0x1E,0x00,0x00,0x00,0x66,0xF7,0xF3});
    if (!Run(m16,c16,v16)) { std::cerr << "MULDIV_DIV16_RUN RAX=0x" << std::hex << c16.Rax() << " RDX=0x" << c16.ReadRegister64(2) << std::dec << "\n"; return false; }
    if ((c16.Rax()&0xFFFFU)!=0x0021U || (c16.ReadRegister64(2)&0xFFFFU)!=0x000AU) { std::cerr << "MULDIV_DIV16_RESULT\n"; return false; }
    std::vector<std::uint8_t> i16={0x66,0xB8,0x18,0xFC,0x66,0xBA,0xFF,0xFF,0x66,0xBB,0x1E,0x00,0x66,0xF7,0xFB};
    if (!Run(m16,c16,i16)) { std::cerr << "MULDIV_IDIV16_RUN RAX=0x" << std::hex << c16.Rax() << " RDX=0x" << c16.ReadRegister64(2) << std::dec << "\n"; return false; }
    if ((c16.Rax()&0xFFFFU)!=0xFFDFU || (c16.ReadRegister64(2)&0xFFFFU)!=0xFFF6U) { std::cerr << "MULDIV_IDIV16_RESULT RAX=0x" << std::hex << c16.Rax() << " RDX=0x" << c16.ReadRegister64(2) << std::dec << "\n"; return false; }

    // 32-bit DIV: EDX:EAX / EBX = 100000 / 30000.
    Memory m32; m32.Map(0x1000,0x3000); Cpu c32; c32.ConnectMemory(&m32);
    std::vector<std::uint8_t> v32; AppendMovR64(v32,0,100000); AppendMovR64(v32,2,0); AppendMovR64(v32,3,30000);
    v32.insert(v32.end(),{0xF7,0xF3});
    if (!Run(m32,c32,v32) || (c32.Rax()&0xFFFFFFFFULL)!=3 || (c32.ReadRegister64(2)&0xFFFFFFFFULL)!=10000) { return false; }

    // 64-bit IDIV: RDX:RAX / RBX = -100 / 7.
    Memory m64; m64.Map(0x1000,0x3000); Cpu c64; c64.ConnectMemory(&m64);
    std::vector<std::uint8_t> v64; AppendMovR64(v64,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(v64,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(v64,3,7);
    v64.insert(v64.end(),{0x48,0xF7,0xFB});
    if (!Run(m64,c64,v64) || c64.Rax()!=0xFFFFFFFFFFFFFFF2ULL || c64.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFEULL) return false;

    // Quotient overflow must raise #DE.
    Memory ov; ov.Map(0x1000,0x3000); Cpu co; co.ConnectMemory(&ov);
    std::vector<std::uint8_t> vo; AppendMovR64(vo,0,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(vo,2,1); AppendMovR64(vo,3,1);
    vo.insert(vo.end(),{0x48,0xF7,0xF3});
    vo.push_back(0xF4);
    if (!ov.Write(0x1000,vo.data(),vo.size())) { return false; }
    co.SetInstructionPointer(0x1000);
    if (co.Run()==0) { return false; }

    // IDIV minimum signed 64-bit / -1 must raise #DE without signed-overflow UB.
    Memory min64; min64.Map(0x1000,0x3000); Cpu cm; cm.ConnectMemory(&min64);
    std::vector<std::uint8_t> vm; AppendMovR64(vm,0,0x8000000000000000ULL); AppendMovR64(vm,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(vm,3,0xFFFFFFFFFFFFFFFFULL);
    vm.insert(vm.end(),{0x48,0xF7,0xFB,0xF4});
    if (!min64.Write(0x1000,vm.data(),vm.size())) { return false; }
    cm.SetInstructionPointer(0x1000);
    if (cm.Run()==0) { return false; }

    Memory divMem32; divMem32.Map(0x1000,0x3000); Cpu divMem32Cpu; divMem32Cpu.ConnectMemory(&divMem32);
    const std::uint32_t div32 = 7;
    if(!divMem32.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&div32),sizeof(div32))) return false;
    std::vector<std::uint8_t> divMem32Code;
    AppendMovR64(divMem32Code,0,100); AppendMovR64(divMem32Code,2,0); AppendMovR64(divMem32Code,7,0x1800);
    divMem32Code.insert(divMem32Code.end(),{0xF7,0x37});
    if(!Run(divMem32,divMem32Cpu,divMem32Code) ||
       (divMem32Cpu.Rax()&0xFFFFFFFFULL)!=14U ||
       (divMem32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=2U) return false;

    Memory divMem8; divMem8.Map(0x1000,0x3000); Cpu divMem8Cpu; divMem8Cpu.ConnectMemory(&divMem8);
    const std::uint8_t div8 = 10;
    if(!divMem8.Write(0x1800,&div8,sizeof(div8))) return false;
    std::vector<std::uint8_t> divMem8Code;
    AppendMovR64(divMem8Code,0,1000); AppendMovR64(divMem8Code,7,0x1800);
    divMem8Code.insert(divMem8Code.end(),{0xF6,0x37});
    if(!Run(divMem8,divMem8Cpu,divMem8Code) ||
       (divMem8Cpu.Rax()&0xFFFFU)!=0x0064U) return false;

    Memory divMem16; divMem16.Map(0x1000,0x3000); Cpu divMem16Cpu; divMem16Cpu.ConnectMemory(&divMem16);
    const std::uint16_t div16 = 7;
    if(!divMem16.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&div16),sizeof(div16))) return false;
    std::vector<std::uint8_t> divMem16Code;
    AppendMovR64(divMem16Code,0,100); AppendMovR64(divMem16Code,2,0); AppendMovR64(divMem16Code,7,0x1800);
    divMem16Code.insert(divMem16Code.end(),{0x66,0xF7,0x37});
    if(!Run(divMem16,divMem16Cpu,divMem16Code) ||
       (divMem16Cpu.Rax()&0xFFFFU)!=14U ||
       (divMem16Cpu.ReadRegister64(2)&0xFFFFU)!=2U) return false;

    Memory idivMem8; idivMem8.Map(0x1000,0x3000); Cpu idivMem8Cpu; idivMem8Cpu.ConnectMemory(&idivMem8);
    const std::uint8_t idiv8 = 7;
    if(!idivMem8.Write(0x1800,&idiv8,sizeof(idiv8))) return false;
    std::vector<std::uint8_t> idivMem8Code;
    AppendMovR64(idivMem8Code,0,0xFFD8ULL); AppendMovR64(idivMem8Code,7,0x1800);
    idivMem8Code.insert(idivMem8Code.end(),{0xF6,0x3F});
    if(!Run(idivMem8,idivMem8Cpu,idivMem8Code) ||
       (idivMem8Cpu.Rax()&0xFFFFU)!=0xFBFBU) return false;

    Memory idivMem32; idivMem32.Map(0x1000,0x3000); Cpu idivMem32Cpu; idivMem32Cpu.ConnectMemory(&idivMem32);
    const std::uint32_t idiv32 = 7;
    if(!idivMem32.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&idiv32),sizeof(idiv32))) return false;
    std::vector<std::uint8_t> idivMem32Code;
    AppendMovR64(idivMem32Code,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(idivMem32Code,2,0xFFFFFFFFFFFFFFFFULL);
    AppendMovR64(idivMem32Code,7,0x1800);
    idivMem32Code.insert(idivMem32Code.end(),{0xF7,0x3F});
    if(!Run(idivMem32,idivMem32Cpu,idivMem32Code) ||
       (idivMem32Cpu.Rax()&0xFFFFFFFFULL)!=0xFFFFFFF2ULL ||
       (idivMem32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0xFFFFFFFEULL) return false;

    Memory idivMem16; idivMem16.Map(0x1000,0x3000); Cpu idivMem16Cpu; idivMem16Cpu.ConnectMemory(&idivMem16);
    const std::uint16_t idiv16 = 7;
    if(!idivMem16.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&idiv16),sizeof(idiv16))) return false;
    std::vector<std::uint8_t> idivMem16Code;
    AppendMovR64(idivMem16Code,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(idivMem16Code,2,0xFFFFFFFFFFFFFFFFULL);
    AppendMovR64(idivMem16Code,7,0x1800);
    idivMem16Code.insert(idivMem16Code.end(),{0x66,0xF7,0x3F});
    if(!Run(idivMem16,idivMem16Cpu,idivMem16Code) ||
       (idivMem16Cpu.Rax()&0xFFFFU)!=0xFFF2U ||
       (idivMem16Cpu.ReadRegister64(2)&0xFFFFU)!=0xFFFEU) return false;

    Memory divMem64; divMem64.Map(0x1000,0x3000); Cpu divMem64Cpu; divMem64Cpu.ConnectMemory(&divMem64);
    const std::uint64_t div64 = 7;
    if(!divMem64.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&div64),sizeof(div64))) return false;
    std::vector<std::uint8_t> divMem64Code;
    AppendMovR64(divMem64Code,0,100); AppendMovR64(divMem64Code,2,0); AppendMovR64(divMem64Code,7,0x1800);
    divMem64Code.insert(divMem64Code.end(),{0x48,0xF7,0x37});
    if(!Run(divMem64,divMem64Cpu,divMem64Code) ||
       divMem64Cpu.Rax()!=14U || divMem64Cpu.ReadRegister64(2)!=2U) return false;

    Memory divMem64Ext; divMem64Ext.Map(0x1000,0x3000); Cpu divMem64ExtCpu; divMem64ExtCpu.ConnectMemory(&divMem64Ext);
    if(!divMem64Ext.Write(0x1900,reinterpret_cast<const std::uint8_t*>(&div64),sizeof(div64))) return false;
    std::vector<std::uint8_t> divMem64ExtCode;
    AppendMovR64(divMem64ExtCode,0,100); AppendMovR64(divMem64ExtCode,2,0); AppendMovR64(divMem64ExtCode,15,0x1900);
    divMem64ExtCode.insert(divMem64ExtCode.end(),{0x49,0xF7,0x37});
    if(!Run(divMem64Ext,divMem64ExtCpu,divMem64ExtCode) ||
       divMem64ExtCpu.Rax()!=14U || divMem64ExtCpu.ReadRegister64(2)!=2U) return false;

    Memory idivMem64; idivMem64.Map(0x1000,0x3000); Cpu idivMem64Cpu; idivMem64Cpu.ConnectMemory(&idivMem64);
    const std::uint64_t idiv64 = 7;
    if(!idivMem64.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&idiv64),sizeof(idiv64))) return false;
    std::vector<std::uint8_t> idivMem64Code;
    AppendMovR64(idivMem64Code,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(idivMem64Code,2,0xFFFFFFFFFFFFFFFFULL);
    AppendMovR64(idivMem64Code,7,0x1800);
    idivMem64Code.insert(idivMem64Code.end(),{0x48,0xF7,0x3F});
    if(!Run(idivMem64,idivMem64Cpu,idivMem64Code) ||
       idivMem64Cpu.Rax()!=0xFFFFFFFFFFFFFFF2ULL ||
       idivMem64Cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFEULL) return false;
    Memory idivMem64Ext; idivMem64Ext.Map(0x1000,0x3000); Cpu idivMem64ExtCpu; idivMem64ExtCpu.ConnectMemory(&idivMem64Ext);
    if(!idivMem64Ext.Write(0x1900,reinterpret_cast<const std::uint8_t*>(&idiv64),sizeof(idiv64))) return false;
    std::vector<std::uint8_t> idivMem64ExtCode;
    AppendMovR64(idivMem64ExtCode,0,0xFFFFFFFFFFFFFF9CULL); AppendMovR64(idivMem64ExtCode,2,0xFFFFFFFFFFFFFFFFULL);
    AppendMovR64(idivMem64ExtCode,15,0x1900);
    idivMem64ExtCode.insert(idivMem64ExtCode.end(),{0x49,0xF7,0x3F});
    if(!Run(idivMem64Ext,idivMem64ExtCpu,idivMem64ExtCode) ||
       idivMem64ExtCpu.Rax()!=0xFFFFFFFFFFFFFFF2ULL ||
       idivMem64ExtCpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFEULL) return false;

    return true;
}

static bool TestDivideFaultsAndBoundaries() {
    auto expectFault = [](Memory& m, Cpu& cpu, const std::vector<std::uint8_t>& code) {
        std::vector<std::uint8_t> program = code;
        program.push_back(0xF4);
        if (!m.Write(0x1000, program.data(), program.size())) return false;
        cpu.SetInstructionPointer(0x1000);
        return cpu.Run() != 0;
    };

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
if (!expectFault(m,cpu,{0xB0,0x00,0xF6,0xF3})) { return false; }
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
if (!expectFault(m,cpu,{0xB8,0x00,0x01,0xB3,0x01,0xF6,0xF3})) { return false; }
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
if (!expectFault(m,cpu,{0x66,0xB8,0x00,0x00,0x66,0xBA,0x00,0x80,0x66,0xBB,0xFF,0xFF,0x66,0xF7,0xFB})) { return false; }
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
if (!expectFault(m,cpu,{0xB8,0x00,0x00,0x00,0x80,0xBA,0xFF,0xFF,0xFF,0xFF,0xBB,0xFF,0xFF,0xFF,0xFF,0xF7,0xFB})) { return false; }
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000000ULL);
        AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,3,0xFFFFFFFFFFFFFFFFULL);
        code.insert(code.end(),{0x48,0xF7,0xFB});
        if (!expectFault(m,cpu,code)) { return false; }
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t zero = 0;
        if (!m.Write(0x1800,&zero,sizeof(zero))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF6,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t zero = 0;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&zero),sizeof(zero))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1); AppendMovR64(code,2,0); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x66,0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t zero = 0;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&zero),sizeof(zero))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1); AppendMovR64(code,2,0); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t zero = 0;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&zero),sizeof(zero))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1); AppendMovR64(code,2,0); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t divisor = 1;
        if (!m.Write(0x1800,&divisor,sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x0100); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF6,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t divisor = 1;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0); AppendMovR64(code,2,1); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x66,0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t divisor = 1;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0); AppendMovR64(code,2,1); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t divisor = 1;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0); AppendMovR64(code,2,1); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xF7,0x37});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t divisor = 0xFF;
        if (!m.Write(0x1800,&divisor,sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFF80ULL); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF6,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t divisor = 0xFFFFFFFFU;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x80000000ULL); AppendMovR64(code,2,0xFFFFFFFFULL); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF7,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t divisor = 0xFFFFFFFFFFFFFFFFULL;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000000ULL); AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xF7,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t divisor = 0xFFFF;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000); AppendMovR64(code,2,0xFFFF); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x66,0xF7,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t divisor = 0xFFFFFFFFU;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x80000000ULL); AppendMovR64(code,2,0xFFFFFFFFULL); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0xF7,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t divisor = 0xFFFFFFFFFFFFFFFFULL;
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000000ULL); AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xF7,0x3F});
        if (!expectFault(m,cpu,code)) return false;
    }
    return true;
}

static bool TestOneOperandMulWidths() {
    Memory m8; m8.Map(0x1000,0x2000); Cpu c8; c8.ConnectMemory(&m8);
    if(!Run(m8,c8,{0xB0,0x10,0xB3,0x10,0xF6,0xE3})) return false;
    if((c8.Rax()&0xFFFFU)!=0x0100U || (c8.Rflags()&(1ULL| (1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory i8; i8.Map(0x1000,0x2000); Cpu ci8; ci8.ConnectMemory(&i8);
    if(!Run(i8,ci8,{0xB0,0xFE,0xB3,0x03,0xF6,0xEB})) return false;
    if((ci8.Rax()&0xFFFFU)!=0xFFFAU || (ci8.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory m16; m16.Map(0x1000,0x2000); Cpu c16; c16.ConnectMemory(&m16);
    std::vector<std::uint8_t> v16; AppendMovR64(v16,0,0x1000ULL); AppendMovR64(v16,3,0x10ULL); v16.insert(v16.end(),{0x66,0xF7,0xE3});
    if(!Run(m16,c16,v16)) return false;
    if((c16.Rax()&0xFFFFU)!=0 || (c16.ReadRegister64(2)&0xFFFFU)!=1 ||
       (c16.Rflags()&(1ULL|(1ULL<<11)))==0) return false;

    Memory m16NoOverflow; m16NoOverflow.Map(0x1000,0x2000); Cpu c16NoOverflow; c16NoOverflow.ConnectMemory(&m16NoOverflow);
    std::vector<std::uint8_t> v16NoOverflow; AppendMovR64(v16NoOverflow,0,3); AppendMovR64(v16NoOverflow,3,7);
    v16NoOverflow.insert(v16NoOverflow.end(),{0x66,0xF7,0xE3});
    if(!Run(m16NoOverflow,c16NoOverflow,v16NoOverflow) ||
       (c16NoOverflow.Rax()&0xFFFFU)!=21U || (c16NoOverflow.ReadRegister64(2)&0xFFFFU)!=0U ||
       (c16NoOverflow.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory m32; m32.Map(0x1000,0x2000); Cpu c32; c32.ConnectMemory(&m32);
    std::vector<std::uint8_t> v32; AppendMovR64(v32,0,0x10000000ULL); AppendMovR64(v32,3,0x10ULL);
    v32.insert(v32.end(),{0xF7,0xE3});
    if(!Run(m32,c32,v32)) return false;
    if((c32.Rax()&0xFFFFFFFFULL)!=0 || (c32.ReadRegister64(2)&0xFFFFFFFFULL)!=1 ||
       (c32.Rflags()&(1ULL|(1ULL<<11)))==0) return false;

    Memory m32NoOverflow; m32NoOverflow.Map(0x1000,0x2000); Cpu c32NoOverflow; c32NoOverflow.ConnectMemory(&m32NoOverflow);
    std::vector<std::uint8_t> v32NoOverflow; AppendMovR64(v32NoOverflow,0,3); AppendMovR64(v32NoOverflow,3,7);
    v32NoOverflow.insert(v32NoOverflow.end(),{0xF7,0xE3});
    if(!Run(m32NoOverflow,c32NoOverflow,v32NoOverflow) ||
       (c32NoOverflow.Rax()&0xFFFFFFFFULL)!=21U || (c32NoOverflow.ReadRegister64(2)&0xFFFFFFFFULL)!=0U ||
       (c32NoOverflow.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory m64; m64.Map(0x1000,0x2000); Cpu c64; c64.ConnectMemory(&m64);
    std::vector<std::uint8_t> v64; AppendMovR64(v64,0,0x0000000100000000ULL); AppendMovR64(v64,3,2);
    v64.insert(v64.end(),{0x48,0xF7,0xE3});
    if(!Run(m64,c64,v64)) return false;
    if(c64.Rax()!=0x0000000200000000ULL || c64.ReadRegister64(2)!=0 ||
       (c64.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory m64Overflow; m64Overflow.Map(0x1000,0x2000); Cpu c64Overflow; c64Overflow.ConnectMemory(&m64Overflow);
    std::vector<std::uint8_t> v64Overflow; AppendMovR64(v64Overflow,0,0x100000000ULL); AppendMovR64(v64Overflow,3,0x100000000ULL);
    v64Overflow.insert(v64Overflow.end(),{0x48,0xF7,0xE3});
    if(!Run(m64Overflow,c64Overflow,v64Overflow) ||
       c64Overflow.Rax()!=0x0000000000000000ULL || c64Overflow.ReadRegister64(2)!=0x0000000000000001ULL ||
       (c64Overflow.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory min32; min32.Map(0x1000,0x2000); Cpu min32Cpu; min32Cpu.ConnectMemory(&min32);
    std::vector<std::uint8_t> min32Code;
    AppendMovR64(min32Code,0,0x80000000ULL); AppendMovR64(min32Code,3,0xFFFFFFFFULL);
    min32Code.insert(min32Code.end(),{0xF7,0xEB});
    if(!Run(min32,min32Cpu,min32Code) ||
       (min32Cpu.Rax()&0xFFFFFFFFULL)!=0x80000000ULL ||
       (min32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0U ||
       (min32Cpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory signed16; signed16.Map(0x1000,0x2000); Cpu signed16Cpu; signed16Cpu.ConnectMemory(&signed16);
    std::vector<std::uint8_t> signed16Code;
    AppendMovR64(signed16Code,0,0xFFFEULL); AppendMovR64(signed16Code,3,3);
    signed16Code.insert(signed16Code.end(),{0x66,0xF7,0xEB});
    if(!Run(signed16,signed16Cpu,signed16Code) ||
       (signed16Cpu.Rax()&0xFFFFU)!=0xFFFAU ||
       (signed16Cpu.ReadRegister64(2)&0xFFFFU)!=0xFFFFU ||
       (signed16Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory signed32; signed32.Map(0x1000,0x2000); Cpu signed32Cpu; signed32Cpu.ConnectMemory(&signed32);
    std::vector<std::uint8_t> signed32Code;
    AppendMovR64(signed32Code,0,0xFFFFFFFEULL); AppendMovR64(signed32Code,3,3);
    signed32Code.insert(signed32Code.end(),{0xF7,0xEB});
    if(!Run(signed32,signed32Cpu,signed32Code) ||
       (signed32Cpu.Rax()&0xFFFFFFFFULL)!=0xFFFFFFFAULL ||
       (signed32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0xFFFFFFFFULL ||
       (signed32Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory signed16Overflow; signed16Overflow.Map(0x1000,0x2000); Cpu signed16OverflowCpu; signed16OverflowCpu.ConnectMemory(&signed16Overflow);
    std::vector<std::uint8_t> signed16OverflowCode;
    AppendMovR64(signed16OverflowCode,0,0x8000ULL); AppendMovR64(signed16OverflowCode,3,0xFFFFULL);
    signed16OverflowCode.insert(signed16OverflowCode.end(),{0x66,0xF7,0xEB});
    if(!Run(signed16Overflow,signed16OverflowCpu,signed16OverflowCode) ||
       (signed16OverflowCpu.Rax()&0xFFFFU)!=0x8000U ||
       (signed16OverflowCpu.ReadRegister64(2)&0xFFFFU)!=0U ||
       (signed16OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory signed64; signed64.Map(0x1000,0x2000); Cpu signed64Cpu; signed64Cpu.ConnectMemory(&signed64);
    std::vector<std::uint8_t> signed64Code;
    AppendMovR64(signed64Code,0,0xFFFFFFFFFFFFFFFEULL); AppendMovR64(signed64Code,3,3);
    signed64Code.insert(signed64Code.end(),{0x48,0xF7,0xEB});
    if(!Run(signed64,signed64Cpu,signed64Code) ||
       signed64Cpu.Rax()!=0xFFFFFFFFFFFFFFFAULL ||
       signed64Cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL ||
       (signed64Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory signed64Overflow; signed64Overflow.Map(0x1000,0x2000); Cpu signed64OverflowCpu; signed64OverflowCpu.ConnectMemory(&signed64Overflow);
    std::vector<std::uint8_t> signed64OverflowCode;
    AppendMovR64(signed64OverflowCode,0,0x8000000000000000ULL); AppendMovR64(signed64OverflowCode,3,0xFFFFFFFFFFFFFFFFULL);
    signed64OverflowCode.insert(signed64OverflowCode.end(),{0x48,0xF7,0xEB});
    if(!Run(signed64Overflow,signed64OverflowCpu,signed64OverflowCode) ||
       signed64OverflowCpu.Rax()!=0x8000000000000000ULL ||
       signed64OverflowCpu.ReadRegister64(2)!=0ULL ||
       (signed64OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory signed32Overflow; signed32Overflow.Map(0x1000,0x2000); Cpu signed32OverflowCpu; signed32OverflowCpu.ConnectMemory(&signed32Overflow);
    std::vector<std::uint8_t> signed32OverflowCode;
    AppendMovR64(signed32OverflowCode,0,0x80000000ULL); AppendMovR64(signed32OverflowCode,3,0xFFFFFFFFULL);
    signed32OverflowCode.insert(signed32OverflowCode.end(),{0xF7,0xEB});
    if(!Run(signed32Overflow,signed32OverflowCpu,signed32OverflowCode) ||
       (signed32OverflowCpu.Rax()&0xFFFFFFFFULL)!=0x80000000ULL ||
       (signed32OverflowCpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0U ||
       (signed32OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory i64; i64.Map(0x1000,0x2000); Cpu ci64; ci64.ConnectMemory(&i64);
    std::vector<std::uint8_t> vi64; AppendMovR64(vi64,0,0xFFFFFFFFFFFFFFFEULL); AppendMovR64(vi64,3,3);
    vi64.insert(vi64.end(),{0x48,0xF7,0xEB});
    if(!Run(i64,ci64,vi64)) return false;
    if(ci64.Rax()!=0xFFFFFFFFFFFFFFFAULL || ci64.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL) return false;
    if((ci64.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory mem16; mem16.Map(0x1000,0x3000); Cpu mem16Cpu; mem16Cpu.ConnectMemory(&mem16);
    const std::uint16_t mul16Operand = 7;
    if(!mem16.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul16Operand),sizeof(mul16Operand))) return false;
    std::vector<std::uint8_t> mem16Code;
    AppendMovR64(mem16Code,0,3); AppendMovR64(mem16Code,7,0x1800);
    mem16Code.insert(mem16Code.end(),{0x66,0xF7,0x27});
    if(!Run(mem16,mem16Cpu,mem16Code) || (mem16Cpu.Rax()&0xFFFFU)!=21U ||
       (mem16Cpu.ReadRegister64(2)&0xFFFFU)!=0U) return false;

    Memory mem8; mem8.Map(0x1000,0x3000); Cpu mem8Cpu; mem8Cpu.ConnectMemory(&mem8);
    const std::uint8_t mul8Operand = 7;
    if(!mem8.Write(0x1800,&mul8Operand,sizeof(mul8Operand))) return false;
    std::vector<std::uint8_t> mem8Code;
    AppendMovR64(mem8Code,0,3); AppendMovR64(mem8Code,7,0x1800);
    mem8Code.insert(mem8Code.end(),{0xF6,0x27});
    if(!Run(mem8,mem8Cpu,mem8Code) || (mem8Cpu.Rax()&0xFFFFU)!=21U ||
       (mem8Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory mem32; mem32.Map(0x1000,0x3000); Cpu mem32Cpu; mem32Cpu.ConnectMemory(&mem32);
    const std::uint32_t mul32Operand = 7;
    if(!mem32.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul32Operand),sizeof(mul32Operand))) return false;
    std::vector<std::uint8_t> mem32Code;
    AppendMovR64(mem32Code,0,3); AppendMovR64(mem32Code,7,0x1800);
    mem32Code.insert(mem32Code.end(),{0xF7,0x27});
    if(!Run(mem32,mem32Cpu,mem32Code) || (mem32Cpu.Rax()&0xFFFFFFFFULL)!=21U ||
       (mem32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0U ||
       (mem32Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory memSigned16; memSigned16.Map(0x1000,0x3000); Cpu memSigned16Cpu; memSigned16Cpu.ConnectMemory(&memSigned16);
    const std::uint16_t imul16Operand = 3;
    if(!memSigned16.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul16Operand),sizeof(imul16Operand))) return false;
    std::vector<std::uint8_t> memSigned16Code;
    AppendMovR64(memSigned16Code,0,0xFFFEULL); AppendMovR64(memSigned16Code,7,0x1800);
    memSigned16Code.insert(memSigned16Code.end(),{0x66,0xF7,0x2F});
    if(!Run(memSigned16,memSigned16Cpu,memSigned16Code) ||
       (memSigned16Cpu.Rax()&0xFFFFU)!=0xFFFAU ||
       (memSigned16Cpu.ReadRegister64(2)&0xFFFFU)!=0xFFFFU ||
       (memSigned16Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory memSigned32; memSigned32.Map(0x1000,0x3000); Cpu memSigned32Cpu; memSigned32Cpu.ConnectMemory(&memSigned32);
    const std::uint32_t imul32Operand = 3;
    if(!memSigned32.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul32Operand),sizeof(imul32Operand))) return false;
    std::vector<std::uint8_t> memSigned32Code;
    AppendMovR64(memSigned32Code,0,0xFFFFFFFEULL); AppendMovR64(memSigned32Code,7,0x1800);
    memSigned32Code.insert(memSigned32Code.end(),{0xF7,0x2F});
    if(!Run(memSigned32,memSigned32Cpu,memSigned32Code) ||
       (memSigned32Cpu.Rax()&0xFFFFFFFFULL)!=0xFFFFFFFAULL ||
       (memSigned32Cpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0xFFFFFFFFULL ||
       (memSigned32Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory memMul64; memMul64.Map(0x1000,0x3000); Cpu memMul64Cpu; memMul64Cpu.ConnectMemory(&memMul64);
    const std::uint64_t mul64Operand = 2;
    if(!memMul64.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul64Operand),sizeof(mul64Operand))) return false;
    std::vector<std::uint8_t> memMul64Code;
    AppendMovR64(memMul64Code,0,0x100000000ULL); AppendMovR64(memMul64Code,7,0x1800);
    memMul64Code.insert(memMul64Code.end(),{0x48,0xF7,0x27});
    if(!Run(memMul64,memMul64Cpu,memMul64Code) ||
       memMul64Cpu.Rax()!=0x200000000ULL || memMul64Cpu.ReadRegister64(2)!=0 ||
       (memMul64Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory memImul8; memImul8.Map(0x1000,0x3000); Cpu memImul8Cpu; memImul8Cpu.ConnectMemory(&memImul8);
    const std::uint8_t imul8Operand = 3;
    if(!memImul8.Write(0x1800,&imul8Operand,sizeof(imul8Operand))) return false;
    std::vector<std::uint8_t> memImul8Code;
    AppendMovR64(memImul8Code,0,0xFE); AppendMovR64(memImul8Code,7,0x1800);
    memImul8Code.insert(memImul8Code.end(),{0xF6,0x2F});
    if(!Run(memImul8,memImul8Cpu,memImul8Code) ||
       (memImul8Cpu.Rax()&0xFFFFU)!=0xFFFAU ||
       (memImul8Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory mem64; mem64.Map(0x1000,0x3000); Cpu mem64Cpu; mem64Cpu.ConnectMemory(&mem64);
    const std::uint64_t imul64Operand = 7;
    if(!mem64.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul64Operand),sizeof(imul64Operand))) return false;
    std::vector<std::uint8_t> mem64Code;
    AppendMovR64(mem64Code,0,0xFFFFFFFFFFFFFFFDULL); AppendMovR64(mem64Code,7,0x1800);
    mem64Code.insert(mem64Code.end(),{0x48,0xF7,0x2F});
    if(!Run(mem64,mem64Cpu,mem64Code) || mem64Cpu.Rax()!=0xFFFFFFFFFFFFFFEBULL ||
       mem64Cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL ||
       (mem64Cpu.Rflags()&(1ULL|(1ULL<<11)))!=0) return false;

    Memory memMul8Overflow; memMul8Overflow.Map(0x1000,0x3000); Cpu memMul8OverflowCpu; memMul8OverflowCpu.ConnectMemory(&memMul8Overflow);
    const std::uint8_t mul8OverflowOperand = 0x10;
    if(!memMul8Overflow.Write(0x1800,&mul8OverflowOperand,sizeof(mul8OverflowOperand))) return false;
    std::vector<std::uint8_t> memMul8OverflowCode;
    AppendMovR64(memMul8OverflowCode,0,0x10); AppendMovR64(memMul8OverflowCode,7,0x1800);
    memMul8OverflowCode.insert(memMul8OverflowCode.end(),{0xF6,0x27});
    if(!Run(memMul8Overflow,memMul8OverflowCpu,memMul8OverflowCode) ||
       (memMul8OverflowCpu.Rax()&0xFFFFU)!=0x0100U ||
       (memMul8OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memMul16Overflow; memMul16Overflow.Map(0x1000,0x3000); Cpu memMul16OverflowCpu; memMul16OverflowCpu.ConnectMemory(&memMul16Overflow);
    const std::uint16_t mul16OverflowOperand = 0x10;
    if(!memMul16Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul16OverflowOperand),sizeof(mul16OverflowOperand))) return false;
    std::vector<std::uint8_t> memMul16OverflowCode;
    AppendMovR64(memMul16OverflowCode,0,0x1000); AppendMovR64(memMul16OverflowCode,7,0x1800);
    memMul16OverflowCode.insert(memMul16OverflowCode.end(),{0x66,0xF7,0x27});
    if(!Run(memMul16Overflow,memMul16OverflowCpu,memMul16OverflowCode) ||
       (memMul16OverflowCpu.Rax()&0xFFFFU)!=0U || (memMul16OverflowCpu.ReadRegister64(2)&0xFFFFU)!=1U ||
       (memMul16OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memMul32Overflow; memMul32Overflow.Map(0x1000,0x3000); Cpu memMul32OverflowCpu; memMul32OverflowCpu.ConnectMemory(&memMul32Overflow);
    const std::uint32_t mul32OverflowOperand = 0x10;
    if(!memMul32Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul32OverflowOperand),sizeof(mul32OverflowOperand))) return false;
    std::vector<std::uint8_t> memMul32OverflowCode;
    AppendMovR64(memMul32OverflowCode,0,0x10000000ULL); AppendMovR64(memMul32OverflowCode,7,0x1800);
    memMul32OverflowCode.insert(memMul32OverflowCode.end(),{0xF7,0x27});
    if(!Run(memMul32Overflow,memMul32OverflowCpu,memMul32OverflowCode) ||
       (memMul32OverflowCpu.Rax()&0xFFFFFFFFULL)!=0U || (memMul32OverflowCpu.ReadRegister64(2)&0xFFFFFFFFULL)!=1U ||
       (memMul32OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memMul64Overflow; memMul64Overflow.Map(0x1000,0x3000); Cpu memMul64OverflowCpu; memMul64OverflowCpu.ConnectMemory(&memMul64Overflow);
    const std::uint64_t mul64OverflowOperand = 0x100000000ULL;
    if(!memMul64Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&mul64OverflowOperand),sizeof(mul64OverflowOperand))) return false;
    std::vector<std::uint8_t> memMul64OverflowCode;
    AppendMovR64(memMul64OverflowCode,0,0x100000000ULL); AppendMovR64(memMul64OverflowCode,7,0x1800);
    memMul64OverflowCode.insert(memMul64OverflowCode.end(),{0x48,0xF7,0x27});
    if(!Run(memMul64Overflow,memMul64OverflowCpu,memMul64OverflowCode) ||
       memMul64OverflowCpu.Rax()!=0U || memMul64OverflowCpu.ReadRegister64(2)!=1U ||
       (memMul64OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memImul16Overflow; memImul16Overflow.Map(0x1000,0x3000); Cpu memImul16OverflowCpu; memImul16OverflowCpu.ConnectMemory(&memImul16Overflow);
    const std::uint16_t imul16OverflowOperand = 0xFFFF;
    if(!memImul16Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul16OverflowOperand),sizeof(imul16OverflowOperand))) return false;
    std::vector<std::uint8_t> memImul16OverflowCode;
    AppendMovR64(memImul16OverflowCode,0,0x8000ULL); AppendMovR64(memImul16OverflowCode,7,0x1800);
    memImul16OverflowCode.insert(memImul16OverflowCode.end(),{0x66,0xF7,0x2F});
    if(!Run(memImul16Overflow,memImul16OverflowCpu,memImul16OverflowCode) ||
       (memImul16OverflowCpu.Rax()&0xFFFFU)!=0x8000U || (memImul16OverflowCpu.ReadRegister64(2)&0xFFFFU)!=0U ||
       (memImul16OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memImul32Overflow; memImul32Overflow.Map(0x1000,0x3000); Cpu memImul32OverflowCpu; memImul32OverflowCpu.ConnectMemory(&memImul32Overflow);
    const std::uint32_t imul32OverflowOperand = 0xFFFFFFFFU;
    if(!memImul32Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul32OverflowOperand),sizeof(imul32OverflowOperand))) return false;
    std::vector<std::uint8_t> memImul32OverflowCode;
    AppendMovR64(memImul32OverflowCode,0,0x80000000ULL); AppendMovR64(memImul32OverflowCode,7,0x1800);
    memImul32OverflowCode.insert(memImul32OverflowCode.end(),{0xF7,0x2F});
    if(!Run(memImul32Overflow,memImul32OverflowCpu,memImul32OverflowCode) ||
       (memImul32OverflowCpu.Rax()&0xFFFFFFFFULL)!=0x80000000ULL || (memImul32OverflowCpu.ReadRegister64(2)&0xFFFFFFFFULL)!=0U ||
       (memImul32OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    Memory memImul64Overflow; memImul64Overflow.Map(0x1000,0x3000); Cpu memImul64OverflowCpu; memImul64OverflowCpu.ConnectMemory(&memImul64Overflow);
    const std::uint64_t imul64OverflowOperand = 0xFFFFFFFFFFFFFFFFULL;
    if(!memImul64Overflow.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&imul64OverflowOperand),sizeof(imul64OverflowOperand))) return false;
    std::vector<std::uint8_t> memImul64OverflowCode;
    AppendMovR64(memImul64OverflowCode,0,0x8000000000000000ULL); AppendMovR64(memImul64OverflowCode,7,0x1800);
    memImul64OverflowCode.insert(memImul64OverflowCode.end(),{0x48,0xF7,0x2F});
    if(!Run(memImul64Overflow,memImul64OverflowCpu,memImul64OverflowCode) ||
       memImul64OverflowCpu.Rax()!=0x8000000000000000ULL || memImul64OverflowCpu.ReadRegister64(2)!=0U ||
       (memImul64OverflowCpu.Rflags()&(1ULL|(1ULL<<11)))!=(1ULL|(1ULL<<11))) return false;

    return true;
}

static bool TestCmpByteForms() {
    Memory m; m.Map(0x1000, 0x3000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 0, 0x1234);
    AppendMovR64(code, 3, 0x12);
    code.insert(code.end(), {0x38, 0xD8, 0x3A, 0xC3, 0x3C, 0x34});
    if (!Run(m, cpu, code)) return false;
    if (cpu.ReadRegister64(0) != 0x1234 || cpu.ReadRegister64(3) != 0x12) return false;
    const std::uint64_t flags = cpu.Rflags();
    if ((flags & 1ULL) != 0) return false;
    if ((flags & (1ULL << 6)) == 0) return false;
    if ((flags & (1ULL << 7)) != 0) return false;

    {
        Memory mem; mem.Map(0x1000, 0x3000); Cpu c; c.ConnectMemory(&mem);
        const std::uint8_t lhs = 5;
        if (!mem.Write(0x1800, &lhs, sizeof(lhs))) return false;
        std::vector<std::uint8_t> v;
        AppendMovR64(v, 0, 5); AppendMovR64(v, 7, 0x1800);
        v.insert(v.end(), {0x38, 0x07});
        if (!Run(mem, c, v)) return false;
        if ((c.Rflags() & (1ULL << 6)) == 0 || (c.Rflags() & 1ULL) != 0) return false;
    }

    {
        Memory mem; mem.Map(0x1000, 0x3000); Cpu c; c.ConnectMemory(&mem);
        const std::uint8_t rhs = 7;
        if (!mem.Write(0x1800, &rhs, sizeof(rhs))) return false;
        std::vector<std::uint8_t> v;
        AppendMovR64(v, 0, 5); AppendMovR64(v, 7, 0x1800);
        v.insert(v.end(), {0x3A, 0x07});
        if (!Run(mem, c, v)) return false;
        const std::uint64_t f = c.Rflags();
        if ((f & 1ULL) == 0 || (f & (1ULL << 6)) != 0 || (f & (1ULL << 7)) == 0) return false;
    }

    return true;
}

static bool TestCmpImmediateForms() {
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        if(!Run(m,cpu,{0xB0,0x05,0x3C,0x07})) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))==0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0x00,0x80,0x66,0x3D,0xFF,0xFF};
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))==0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x00,0x00,0x00,0x80,0x3D,0x01,0x00,0x00,0x00};
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)!=0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))!=0 || (flags&(1ULL<<11))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x48,0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                        0x48,0x3D,0xFF,0xFF,0xFF,0xFF};
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))!=0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0x00,0x00,0x66,0x83,0xF8,0xFF};
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))!=0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint32_t value=1;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x83,0x3F,0x02});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))==0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x48,0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x83,0xF8,0xFF};
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))!=0 || (flags&(1ULL<<11))!=0) return false;
    }
    return true;
}

static bool TestGroup1ImmediateWidths() {
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x48,0xB8,0x01,0,0,0,0,0,0,0,0x48,0x83,0xC0,0xFF};
        if(!Run(m,cpu,code) || cpu.Rax()!=0 || (cpu.Rflags()&(1ULL<<6))==0 || (cpu.Rflags()&1ULL)==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0,0,0,0,0x83,0xE8,0xFF};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFFFFFULL)!=1ULL ||
           (cpu.Rflags()&1ULL)==0 || (cpu.Rflags()&(1ULL<<6))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0xF0,0xF0,0x66,0x83,0xE0,0xFF};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFULL)!=0xF0F0ULL ||
           (cpu.Rflags()&1ULL)!=0 || (cpu.Rflags()&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x48,0xB8,0x00,0,0,0,0,0,0,0,0x48,0x83,0xF0,0xFF};
        if(!Run(m,cpu,code) || cpu.Rax()!=0xFFFFFFFFFFFFFFFFULL ||
           (cpu.Rflags()&(1ULL<<7))==0 || (cpu.Rflags()&(1ULL<<6))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags()|1ULL);
        std::vector<std::uint8_t> code={0x48,0xB8,0,0,0,0,0,0,0,0,0x48,0x83,0xD0,0xFF};
        if(!Run(m,cpu,code) || cpu.Rax()!=0 ||
           (cpu.Rflags()&(1ULL<<6))==0 || (cpu.Rflags()&1ULL)==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags()|1ULL);
        std::vector<std::uint8_t> code={0xB8,0,0,0,0,0x83,0xD8,0xFF};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFFFFFULL)!=0 ||
           (cpu.Rflags()&(1ULL<<6))==0 || (cpu.Rflags()&1ULL)==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=1;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x81,0x07,0xFF,0xFF,0xFF,0xFF});
        if(!Run(m,cpu,code) || cpu.Rax()!=0 || (cpu.Rflags()&(1ULL<<6))==0) return false;
        std::uint64_t out=1;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        if(out!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint16_t value=2;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x66,0x81,0x2F,0x01,0x00});
        if(!Run(m,cpu,code)) return false;
        std::uint16_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        return out==1 && (cpu.Rflags()&1ULL)==0;
    }
    return true;
}


static bool TestGroup1FlagMatrix() {
    const std::uint64_t CF = 1ULL;
    const std::uint64_t PF = 1ULL << 2;
    const std::uint64_t AF = 1ULL << 4;
    const std::uint64_t ZF = 1ULL << 6;
    const std::uint64_t SF = 1ULL << 7;
    const std::uint64_t OF = 1ULL << 11;

    // 8-bit ADC: 0x7f + 0 + CF -> 0x80, signed overflow, AF set, PF set.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code={0xB0,0x7F,0xB3,0x00,0x10,0xD8};
        if(!Run(m,cpu,code)) return false;
        const auto f=cpu.Rflags();
        if((cpu.ReadRegister64(0)&0xFFU)!=0x80U) return false;
        if((f&(CF|PF|AF|SF|OF))!=(AF|SF|OF) || (f&ZF)!=0) return false;
    }

    // 8-bit SBB: 0x80 - 0 - CF -> 0x7f, signed overflow, AF set, PF clear.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code={0xB0,0x80,0xB3,0x00,0x18,0xD8};
        if(!Run(m,cpu,code)) return false;
        const auto f=cpu.Rflags();
        if((cpu.ReadRegister64(0)&0xFFU)!=0x7FU) return false;
        if((f&(CF|PF|AF|SF|OF|ZF))!=(AF|OF)) return false;
    }

    // 16-bit ADC immediate: 0x7fff + 0 + CF -> 0x8000.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code={0x66,0xB8,0xFF,0x7F,0x66,0x83,0xD0,0x00};
        if(!Run(m,cpu,code)) return false;
        const auto f=cpu.Rflags();
        if((cpu.Rax()&0xFFFFU)!=0x8000U || (f&(OF|SF))!=(OF|SF) || (f&ZF)!=0) return false;
    }

    // 32-bit SBB immediate: 0x80000000 - 0 - CF -> 0x7fffffff.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code={0xB8,0x00,0x00,0x00,0x80,0x83,0xD8,0x00};
        if(!Run(m,cpu,code)) return false;
        const auto f=cpu.Rflags();
        if((cpu.Rax()&0xFFFFFFFFULL)!=0x7FFFFFFFULL || (f&(OF|AF|PF))!=(OF|AF|PF) || (f&(CF|SF|ZF))!=0) return false;
    }

    // 64-bit ADC immediate: 0xffff...ffff + 0 + CF -> 1 with carry, no signed overflow.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code={0x48,0xB8,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                                        0x48,0x83,0xD0,0x00};
        if(!Run(m,cpu,code)) return false;
        const auto f=cpu.Rflags();
        if(cpu.Rax()!=0x0000000000000000ULL) return false;
        if((f&(CF|ZF|PF))!=(CF|ZF|PF) || (f&(OF|SF))!=0) return false;
    }

    // 8-bit ADC memory: 0xff + 0 + CF -> 0 with carry, zero, AF and even parity.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint8_t value=0xFF;
        if(!m.Write(0x1800,&value,1)) return false;
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x10,0x0F}); // ADC byte [RDI],CL (CL=0)
        if(!Run(m,cpu,code)) return false;
        std::uint8_t out=0;
        if(!m.Read(0x1800,&out,1) || out!=0) return false;
        const auto f=cpu.Rflags();
        if((f&(CF|ZF|PF|AF))!=(CF|ZF|PF|AF) || (f&OF)!=0) return false;
    }

    // 8-bit SBB memory: 0x00 - 0 - CF -> 0xff with borrow, sign and AF.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint8_t value=0x00;
        if(!m.Write(0x1800,&value,1)) return false;
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x18,0x0F}); // SBB byte [RDI],CL (CL=0)
        if(!Run(m,cpu,code)) return false;
        std::uint8_t out=0;
        if(!m.Read(0x1800,&out,1) || out!=0xFF) return false;
        const auto f=cpu.Rflags();
        if((f&(CF|AF|SF|PF))!=(CF|AF|SF|PF) || (f&(ZF|OF))!=0) return false;
    }

    // 64-bit SBB memory: 0 - 1 - CF -> -2, borrow and sign set.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=0;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x81,0x1F,0x01,0x00,0x00,0x00});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        const auto f=cpu.Rflags();
        if(out!=0xFFFFFFFFFFFFFFFEULL || (f&(CF|SF))!=(CF|SF) || (f&(ZF|OF))!=0) return false;
    }

    return true;
}



static bool TestGroup1FullImmediateForms() {
    // 64-bit 81 /4: imm32 is sign-extended before AND.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x48,0xB8,0x21,0x43,0x65,0x87,0x78,0x56,0x34,0x12,
                                        0x48,0x81,0xE0,0x00,0x00,0x00,0x80};
        if(!Run(m,cpu,code) || cpu.Rax()!=0x1234567880000000ULL) return false;
    }
    // 32-bit 81 /6: full imm32 XOR.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0xF0,0x0F,0xF0,0x0F,0x81,0xF0,0xF0,0x0F,0xF0,0x0F};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFFFFFULL)!=0x00000000ULL ||
           (cpu.Rflags()&(1ULL<<6))==0) return false;
    }
    // 16-bit 81 /7: CMP full imm16, unequal with borrow.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0x34,0x12,0x66,0x81,0xF8,0x35,0x12};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFU)!=0x1234U ||
           (cpu.Rflags()&1ULL)==0 || (cpu.Rflags()&(1ULL<<6))!=0) return false;
    }
    return true;
}

static bool TestGroup1RexAndMemory() {
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0xFFFFFFFFFFFFFFFFULL);
        code.insert(code.end(),{0x49,0x83,0xC0,0x01});
        if(!Run(m,cpu,code) || cpu.ReadRegister64(8)!=0 ||
           (cpu.Rflags()&(1ULL| (1ULL<<6)))!=(1ULL|(1ULL<<6))) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=0x7FFFFFFFFFFFFFFFULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,8,0x1800);
        code.insert(code.end(),{0x49,0x83,0x00,0x01});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        if(out!=0x8000000000000000ULL || (cpu.Rflags()&((1ULL<<7)|(1ULL<<11)))!=((1ULL<<7)|(1ULL<<11))) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,9,0);
        code.insert(code.end(),{0xF9,0x49,0x83,0xD9,0x00});
        if(!Run(m,cpu,code) || cpu.ReadRegister64(9)!=0xFFFFFFFFFFFFFFFFULL ||
           (cpu.Rflags()&(1ULL| (1ULL<<7)))!=(1ULL|(1ULL<<7))) return false;
    }
    return true;
}

static bool TestCmpUnequalFlags() {
    Memory m1; m1.Map(0x1000,0x2000); Cpu c1; c1.ConnectMemory(&m1);
    std::vector<std::uint8_t> code1;
    AppendMovR64(code1,0,5); AppendMovR64(code1,3,7);
    code1.insert(code1.end(),{0x39,0xD8});
    if(!Run(m1,c1,code1)) return false;
    const std::uint64_t f1=c1.Rflags();
    if((f1&1ULL)==0 || (f1&(1ULL<<6))!=0 || (f1&(1ULL<<7))==0 || (f1&(1ULL<<11))!=0) return false;

    Memory m2; m2.Map(0x1000,0x2000); Cpu c2; c2.ConnectMemory(&m2);
    std::vector<std::uint8_t> code2;
    AppendMovR64(code2,0,0x80000000ULL); AppendMovR64(code2,3,1);
    code2.insert(code2.end(),{0x39,0xD8});
    if(!Run(m2,c2,code2)) return false;
    const std::uint64_t f2=c2.Rflags();
    if((f2&1ULL)!=0 || (f2&(1ULL<<6))!=0 || (f2&(1ULL<<7))!=0 || (f2&(1ULL<<11))==0) return false;

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t lhs = 5;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&lhs),sizeof(lhs))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,7); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x66,0x39,0x07});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))==0 || (flags&(1ULL<<11))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t lhs = 0x80000000U;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&lhs),sizeof(lhs))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x39,0x07});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)!=0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))!=0 || (flags&(1ULL<<11))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t lhs = 5;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&lhs),sizeof(lhs))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,7); AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x39,0x07});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t flags=cpu.Rflags();
        if((flags&1ULL)==0 || (flags&(1ULL<<6))!=0 || (flags&(1ULL<<7))==0 || (flags&(1ULL<<11))!=0) return false;
    }
    // CMP AF/PF unequal coverage.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x10); AppendMovR64(code,3,0x01);
        code.insert(code.end(),{0x39,0xD8});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t f=cpu.Rflags();
        if((f&1ULL)!=0 || (f&(1ULL<<6))!=0 || (f&(1ULL<<7))!=0 || (f&(1ULL<<11))!=0) return false;
        if((f&(1ULL<<4))==0 || (f&(1ULL<<2))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x00); AppendMovR64(code,3,0x01);
        code.insert(code.end(),{0x39,0xD8});
        if(!Run(m,cpu,code)) return false;
        const std::uint64_t f=cpu.Rflags();
        if((f&1ULL)==0 || (f&(1ULL<<6))!=0 || (f&(1ULL<<7))==0 || (f&(1ULL<<11))!=0) return false;
        if((f&(1ULL<<4))==0 || (f&(1ULL<<2))==0) return false;
    }

    return true;
}

static bool TestGroup1ExtendedAllWidths() {
    auto run64 = [](std::uint64_t initial, std::uint8_t group, std::uint8_t imm,
                    bool carryIn, std::uint64_t expected, bool writeBack) {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&initial),sizeof(initial))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        if(carryIn) cpu.SetRflags(cpu.Rflags() | 1ULL);
        code.insert(code.end(),{0x4F,0x83,static_cast<std::uint8_t>(0x04U | (group<<3)),0xA3,imm});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        return writeBack ? out==expected : out==initial;
    };

    if(!run64(1,0,0x7F,false,0x80,true)) return false;       // ADD
    if(!run64(0x10,1,0x0F,false,0x1F,true)) return false;    // OR
    if(!run64(0x7F,2,0x00,true,0x80,true)) return false;     // ADC
    if(!run64(0x80,3,0x00,true,0x7F,true)) return false;     // SBB
    if(!run64(0xFF,4,0x0F,false,0x0F,true)) return false;   // AND
    if(!run64(2,5,0xFF,false,3,true)) return false;          // SUB with sign-extended -1
    if(!run64(0xF0,6,0x0F,false,0xFF,true)) return false;   // XOR
    if(!run64(5,7,0x06,false,5,false)) return false;         // CMP

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t initial=0x01;
        if(!m.Write(0x1808,&initial,1)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        code.insert(code.end(),{0x47,0x80,0x04,0xA3,0xFF}); // ADD byte, -1
        if(!Run(m,cpu,code)) return false;
        std::uint8_t out=0; if(!m.Read(0x1808,&out,1) || out!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t initial=1;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&initial),2)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        code.insert(code.end(),{0x66,0x47,0x83,0x04,0xA3,0xFF}); // ADD word, -1
        if(!Run(m,cpu,code)) return false;
        std::uint16_t out=0; if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),2) || out!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t initial=1;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&initial),4)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        code.insert(code.end(),{0x47,0x83,0x04,0xA3,0xFF}); // ADD dword, -1
        if(!Run(m,cpu,code)) return false;
        std::uint32_t out=0; if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),4) || out!=0) return false;
    }

    {
        // 32-bit address-size override with extended SIB base/index.
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint32_t value=5;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        // Use a 32-bit address whose low 32 bits point at 0x1808.
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x67,0x47,0x83,0x04,0xA3,0x01});
        if(!Run(m,cpu,code)) return false;
        std::uint32_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=6) return false;
    }

    {
        // SIB disp8 addressing: [R11 + R12*4 + 8].
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=7;
        if(!m.Write(0x1810,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4F,0x83,0x44,0xA3,0x08,0x01});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1810,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=8) return false;
    }

    {
        // SIB disp32 addressing: [R11 + R12*4 + 0x20].
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=9;
        if(!m.Write(0x1828,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800); AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4F,0x83,0x84,0xA3,0x20,0x00,0x00,0x00,0x01});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1828,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=10) return false;
    }

    return true;
}

static bool TestShift32ZeroCount() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    cpu.SetRflags(1ULL | (1ULL<<11) | (1ULL<<6));
    std::vector<std::uint8_t> code={0xB8,0xEF,0xCD,0xAB,0x89,0xB9,0x00,0x00,0x00,0x00,0xD3,0xE0};
    if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.Rax())!=0x89ABCDEFU) return false;
    return cpu.Rflags()==(1ULL | (1ULL<<11) | (1ULL<<6));
}

static bool TestShift16ExtendedRegister() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,15,0x8000ULL);
    code.insert(code.end(),{0x66,0x41,0xD1,0xE7}); // SHL R15W,1
    if(!Run(m,cpu,code) || (cpu.ReadRegister64(15)&0xFFFFU)!=0) return false;
    if((cpu.Rflags()&1ULL)==0 || (cpu.Rflags()&(1ULL<<11))==0) return false;
    return true;
}

static bool TestShift32Parity() {
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x03,0x00,0x00,0x00,0xC1,0xE0,0x01};
        if(!Run(m,cpu,code) || cpu.ReadRegister64(0)!=6ULL) return false;
        if((cpu.Rflags() & (1ULL<<2))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x01,0x00,0x00,0x00,0xC1,0xE8,0x01};
        if(!Run(m,cpu,code) || cpu.ReadRegister64(0)!=0ULL) return false;
        if((cpu.Rflags() & (1ULL<<2))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x83,0x00,0x00,0x80,0xC1,0xF8,0x01};
        if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.ReadRegister64(0))!=0xC0000041U) return false;
        if((cpu.Rflags() & (1ULL<<2))==0) return false;
    }
    return true;
}

static bool TestShiftLeft64Parity() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,0,1);
    code.insert(code.end(),{0x48,0xD1,0xE0}); // SHL RAX,1
    if(!Run(m,cpu,code) || cpu.Rax()!=2ULL) return false;
    // 0x02 has odd parity, so PF must be clear.
    return (cpu.Rflags()&(1ULL<<2))==0;
}

static bool TestShiftLeft64CLFlags() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,1,1);
    AppendMovR64(code,0,0x8000000000000000ULL);
    code.insert(code.end(),{0x48,0xD3,0xE0}); // SHL RAX,CL
    if(!Run(m,cpu,code) || cpu.Rax()!=0ULL) return false;
    if((cpu.Rflags()&(1ULL<<2))==0) return false;
    if((cpu.Rflags()&1ULL)==0) return false;
    return true;
}

static bool TestShiftRight64Forms() {
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        code.insert(code.end(),{0x48,0xD1,0xE8}); // SHR RAX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=0x4000000000000000ULL) return false;
        const auto f=cpu.Rflags();
        if((f&1ULL)==0 || (f&(1ULL<<11))==0 || (f&(1ULL<<7))!=0 ||
           (f&(1ULL<<2))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        code.insert(code.end(),{0x48,0xD1,0xF8}); // SAR RAX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=0xC000000000000000ULL) return false;
        const auto f=cpu.Rflags();
        if((f&1ULL)==0 || (f&(1ULL<<11))!=0 || (f&(1ULL<<7))==0 ||
           (f&(1ULL<<2))==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xF000000000000001ULL);
        code.insert(code.end(),{0x48,0xC1,0xE8,0x04}); // SHR RAX,4
        if(!Run(m,cpu,code) || cpu.Rax()!=0x0F00000000000000ULL) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,1,1);
        AppendMovR64(code,0,0x8000000000000000ULL);
        code.insert(code.end(),{0x48,0xD3,0xE8}); // SHR RAX,CL
        if(!Run(m,cpu,code) || cpu.Rax()!=0x4000000000000000ULL) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x8000000000000001ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xD1,0x2F}); // SHR qword [RDI],1
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        return m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) &&
               out==0x4000000000000000ULL;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x8000000000000001ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0xC1,0x3F,0x01}); // SAR qword [RDI],1
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        return m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) &&
               out==0xC000000000000000ULL;
    }
    {
        // SAR r64,CL must consume CL and preserve the sign.
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,1,4);
        AppendMovR64(code,0,0x8000000000000000ULL);
        code.insert(code.end(),{0x48,0xD3,0xF8}); // SAR RAX,CL
        if(!Run(m,cpu,code) || cpu.Rax()!=0xF800000000000000ULL) return false;
    }

    {
        // A zero shift count must leave the operand and flags untouched.
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        AppendMovR64(code,1,0);
        code.insert(code.end(),{0x48,0xD3,0xE8}); // SHR RAX,CL (CL=0)
        const std::uint64_t before=cpu.Rflags();
        if(!Run(m,cpu,code) || cpu.Rax()!=0x8000000000000001ULL) return false;
        if(cpu.Rflags()!=before) return false;
    }

    {
        // Extended register form: R15 with REX.B.
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,15,0x8000000000000001ULL);
        code.insert(code.end(),{0x49,0xD1,0xEF}); // SHR R15,1
        if(!Run(m,cpu,code) || cpu.ReadRegister64(15)!=0x4000000000000000ULL) return false;
    }

    return true;
}

static bool TestLeaExtendedNoBase() {
    Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,12,2);
    code.insert(code.end(),{0x67,0x42,0x8D,0x04,0xA5,0x00,0x18,0x00,0x00});
    if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFFFFFULL)!=0x1808ULL) return false;
    return true;
}

static bool TestLeaExtendedAddressing() {
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4B,0x8D,0x04,0xA3}); // LEA RAX,[R11+R12*4]
        if(!Run(m,cpu,code) || cpu.Rax()!=0x1808ULL) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4B,0x8D,0x44,0xA3,0x08}); // LEA RAX,[R11+R12*4+8]
        if(!Run(m,cpu,code) || cpu.Rax()!=0x1810ULL) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x67,0x43,0x8D,0x04,0xA3}); // LEA EAX,[R11D+R12D*4]
        if(!Run(m,cpu,code) || cpu.Rax()!=0x1808ULL) return false;
    }
    return true;
}

static bool TestRotate32ExtendedForms() {
    Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,15,0x80000001ULL);
    code.insert(code.end(),{0x41,0xD1,0xC7}); // ROL R15D,1
    if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.ReadRegister64(15))!=3U) return false;
    if((cpu.Rflags()&1ULL)==0 || (cpu.Rflags()&(1ULL<<11))==0) return false;
    return true;
}

static bool TestRotate32ZeroCount() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    cpu.SetRflags(1ULL | (1ULL<<11) | (1ULL<<6));
    std::vector<std::uint8_t> code={0xB8,0xEF,0xCD,0xAB,0x89,0xB9,0x00,0x00,0x00,0x00,0xD3,0xC0};
    if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.Rax())!=0x89ABCDEFU) return false;
    return cpu.Rflags()==(1ULL | (1ULL<<11) | (1ULL<<6));
}

static bool TestRotate64ZeroCount() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    cpu.SetRflags(1ULL | (1ULL<<11) | (1ULL<<6));
    std::vector<std::uint8_t> code;
    AppendMovR64(code,0,0x0123456789ABCDEFULL);
    AppendMovR64(code,1,0);
    code.insert(code.end(),{0x48,0xD3,0xC0}); // ROL RAX,CL where CL=0
    if(!Run(m,cpu,code) || cpu.Rax()!=0x0123456789ABCDEFULL) return false;
    return cpu.Rflags()==(1ULL | (1ULL<<11) | (1ULL<<6));
}

static bool TestRotate64Forms() {
    const std::uint64_t CF=1ULL, OF=1ULL<<11;
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        code.insert(code.end(),{0x48,0xD1,0xC0}); // ROL RAX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=3ULL) return false;
        if((cpu.Rflags()&(CF|OF))!=(CF|OF)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        code.insert(code.end(),{0x48,0xD1,0xC8}); // ROR RAX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=0xC000000000000000ULL) return false;
        if((cpu.Rflags()&CF)==0 || (cpu.Rflags()&OF)!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags()|CF);
        std::vector<std::uint8_t> code; AppendMovR64(code,8,0x8000000000000000ULL);
        code.insert(code.end(),{0x49,0xD1,0xD0}); // RCL R8,1
        if(!Run(m,cpu,code)) return false;
        if(cpu.ReadRegister64(8)!=1ULL || (cpu.Rflags()&CF)==0 || (cpu.Rflags()&OF)==0) {
            std::cerr << "RCL R8 result=0x" << std::hex << cpu.ReadRegister64(8) << " RFLAGS=0x" << cpu.Rflags() << std::dec << "\n";
            return false;
        }
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,1,1);
        AppendMovR64(code,0,1);
        code.insert(code.end(),{0x48,0xD3,0xC0}); // ROL RAX,CL
        if(!Run(m,cpu,code) || cpu.Rax()!=2ULL) return false;
    }
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


static bool TestGroup1ExtendedAddressing() {
    // REX.X/REX.B SIB addressing: [R11 + R12*4].
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value = 5;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4F,0x83,0x04,0xA3,0x01});
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        return out==6;
    }

    // 64-bit RIP-relative addressing for Group1 /0.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value = 5;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code={
            0x48,0x83,0x05,0xF9,0x07,0x00,0x00,0x01
        };
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out))) return false;
        return out==6;
    }
}


static bool TestDoubleShiftExtendedForms() {
    const std::uint64_t CF = 1ULL;
    const std::uint64_t OF = 1ULL << 11;

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        AppendMovR64(code,3,3);
        code.insert(code.end(),{0x48,0x0F,0xA4,0xD8,0x01}); // SHLD RAX,RBX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=2ULL) return false;
        if((cpu.Rflags()&(CF|OF))!=(CF|OF)) return false;
    }

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x8000000000000001ULL);
        AppendMovR64(code,3,3);
        code.insert(code.end(),{0x48,0x0F,0xAC,0xD8,0x01}); // SHRD RAX,RBX,1
        if(!Run(m,cpu,code) || cpu.Rax()!=0xC000000000000000ULL) return false;
        if((cpu.Rflags()&CF)==0 || (cpu.Rflags()&OF)!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x123456789ABCDEF0ULL);
        AppendMovR64(code,9,0x0FEDCBA987654321ULL);
        AppendMovR64(code,1,4);
        code.insert(code.end(),{0x4C,0x0F,0xA5,0xC8}); // SHLD RAX,R9,CL
        if(!Run(m,cpu,code) || cpu.Rax()!=0x23456789ABCDEF00ULL) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x8000000000000001ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,15,0x1800);
        AppendMovR64(code,9,3);
        code.insert(code.end(),{0x4D,0x0F,0xA4,0x0F,0x01}); // SHLD [R15],R9,1
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=2ULL) return false;
        if((cpu.Rflags()&(CF|OF))!=(CF|OF)) return false;
    }

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x01,0x00,0x00,0x80,0xBB,0x03,0x00,0x00,0x00,
            0x0F,0xA4,0xD8,0x01};
        if(!Run(m,cpu,code) || static_cast<std::uint32_t>(cpu.Rax())!=2U) return false;
        if((cpu.Rflags()&(CF|OF))!=(CF|OF)) return false;
    }

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0x01,0x80,0x66,0xBB,0x03,0x00,
            0x66,0x0F,0xAC,0xD8,0x01};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFU)!=0xC000U) return false;
        if((cpu.Rflags()&CF)==0 || (cpu.Rflags()&OF)!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(CF|OF|(1ULL<<6));
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x123456789ABCDEF0ULL);
        AppendMovR64(code,1,0);
        code.insert(code.end(),{0x48,0x0F,0xA5,0xC8});
        if(!Run(m,cpu,code) || cpu.Rax()!=0x123456789ABCDEF0ULL) return false;
        if (cpu.Rflags()!=(CF|OF|(1ULL<<6))) return false; return true;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t value=0x80000001U;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,15,0x1800);
        AppendMovR64(code,9,3);
        code.insert(code.end(),{0x45,0x0F,0xA4,0x0F,0x01}); // SHLD [R15D],R9D,1
        if(!Run(m,cpu,code)) { std::cerr << "SHLD32 memory Run failed\\n"; return false; }
        std::uint32_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=2U) { std::cerr << "SHLD32 memory out=0x" << std::hex << out << "\\n"; return false; }
        if((cpu.Rflags()&(CF|OF))!=(CF|OF)) { std::cerr << "SHLD32 memory flags=0x" << std::hex << cpu.Rflags() << "\\n"; return false; }
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint16_t value=0x8001U;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,15,0x1800);
        AppendMovR64(code,9,3);
        code.insert(code.end(),{0x66,0x45,0x0F,0xAC,0x0F,0x01}); // SHRD word [R15],R9W,1
        if(!Run(m,cpu,code)) { std::cerr << "SHRD16 memory Run failed\\n"; return false; }
        std::uint16_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) || out!=0xC000U) { std::cerr << "SHRD16 memory out=0x" << std::hex << out << "\\n"; return false; }
        if((cpu.Rflags()&CF)==0 || (cpu.Rflags()&OF)!=0) { std::cerr << "SHRD16 memory flags=0x" << std::hex << cpu.Rflags() << "\\n"; return false; }
    }
}


static bool TestBitMemoryForms() {
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t first=0x4ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&first),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        AppendMovR64(code,1,2);
        code.insert(code.end(),{0x48,0x0F,0xA3,0x0F}); // BT [RDI],RCX
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t first=0x1ULL, second=0x0ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&first),8) ||
           !m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&second),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        AppendMovR64(code,1,66);
        code.insert(code.end(),{0x48,0x0F,0xAB,0x0F}); // BTS [RDI],RCX
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)!=0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),8) || out!=0x4ULL) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t first=0x1ULL, second=0x4ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&first),8) ||
           !m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&second),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        AppendMovR64(code,1,66);
        code.insert(code.end(),{0x48,0x0F,0xB3,0x0F}); // BTR [RDI],RCX
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),8) || out!=0x0ULL) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t first=0x1ULL, second=0x4ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&first),8) ||
           !m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&second),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        AppendMovR64(code,1,66);
        code.insert(code.end(),{0x48,0x0F,0xBB,0x0F}); // BTC [RDI],RCX
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1808,reinterpret_cast<std::uint8_t*>(&out),8) || out!=0x0ULL) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t value=0x80U;
        if(!m.Write(0x1807,&value,1)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1808);
        AppendMovR64(code,1,0xFFFFFFFFFFFFFFFFULL); // signed bit offset -1
        code.insert(code.end(),{0x48,0x0F,0xA3,0x0F}); // BT [RDI],RCX
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x2ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x0F,0xBA,0x27,0x01}); // BT [RDI],1
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x0F,0xBA,0x2F,0x01}); // BTS [RDI],1
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)!=0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),8) || out!=2ULL) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=2;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x0F,0xBA,0x37,0x01}); // BTR [RDI],1
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),8) || out!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=2;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x0F,0xBA,0x3F,0x01}); // BTC [RDI],1
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)==0) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),8) || out!=0) return false;
    }

    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t value=0x80000000U, next=0U;
        if(!m.Write(0x1810,reinterpret_cast<const std::uint8_t*>(&value),4) ||
           !m.Write(0x1814,reinterpret_cast<const std::uint8_t*>(&next),4)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,4);
        AppendMovR64(code,9,32);
        code.insert(code.end(),{0x47,0x0F,0xAB,0x0C,0xA3}); // BTS [R11+R12*4],ECX (bit 32 -> next dword)
        if(!Run(m,cpu,code) || (cpu.Rflags()&1ULL)!=0) return false;
        std::uint32_t out=0;
        if(!m.Read(0x1810,reinterpret_cast<std::uint8_t*>(&out),4) || out!=0x80000000U) return false;
        if(!m.Read(0x1814,reinterpret_cast<std::uint8_t*>(&out),4) || out!=1U) return false;
    }

    return true;
}

static bool TestGroup1QwordImmediateMemory() {
    struct Case { std::uint8_t group; std::uint64_t initial; std::uint64_t expected; };
    const Case cases[] = {
        {0, 0x10ULL, 0x0FULL},                         // ADD
        {1, 0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}, // OR
        {2, 0x10ULL, 0x0FULL},                         // ADC, CF clear
        {3, 0xFFFFFFFFFFFFFFF0ULL, 0xFFFFFFFFFFFFFFF1ULL}, // SBB
        {4, 0xF0ULL, 0xF0ULL},                         // AND
        {5, 0x10ULL, 0x11ULL},                         // SUB
        {6, 0x10ULL, 0xFFFFFFFFFFFFFFEFULL},           // XOR
        {7, 0x10ULL, 0x10ULL}                          // CMP
    };
    for (const auto& tc : cases) {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        if (!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&tc.initial),8)) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x81,static_cast<std::uint8_t>(0x07U | (tc.group<<3)),0xFF,0xFF,0xFF,0xFF});
        if (!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if (!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),8)) return false;
        if (out!=tc.expected) return false;
    }
    return true;
}

static bool TestTestRmRegForms() {
    
    constexpr std::uint64_t CF=1ULL, PF=1ULL<<2, AF=1ULL<<4, ZF=1ULL<<6, SF=1ULL<<7, OF=1ULL<<11;
    // TEST r/m,r must not modify operands; CF/OF/AF are cleared and ZF/SF/PF reflect the result.
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,0,0xF0); AppendMovR64(code,3,0x0F);
        code.insert(code.end(),{0x84,0xD8});
        if(!Run(m,cpu,code) || cpu.Rax()!=0xF0 || cpu.ReadRegister64(3)!=0x0F) return false;
        if((cpu.Rflags() & (CF|OF|AF|ZF|SF|PF))!=(ZF|PF)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,0,0x8001); AppendMovR64(code,3,0x8001);
        code.insert(code.end(),{0x66,0x85,0xD8});
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFU)!=0x8001U) return false;
        if((cpu.Rflags()&(ZF|SF|PF))!=SF || (cpu.Rflags()&(CF|OF|AF))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code; AppendMovR64(code,0,0x80000001ULL); AppendMovR64(code,3,0xFFFFFFFFULL);
        code.insert(code.end(),{0x85,0xD8});
        if(!Run(m,cpu,code) || cpu.ReadRegister64(0)!=0x80000001ULL) return false;
        if((cpu.Rflags()&(ZF|SF|PF))!=SF || (cpu.Rflags()&(CF|OF|AF))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x8000000000000001ULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),8)) return false;
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800); AppendMovR64(code,0,0xFFFFFFFFFFFFFFFFULL);
        code.insert(code.end(),{0x48,0x85,0x07});
        if(!Run(m,cpu,code) || cpu.Rax()!=0xFFFFFFFFFFFFFFFFULL) return false;
        if((cpu.Rflags()&(ZF|SF|PF))!=SF || (cpu.Rflags()&(CF|OF|AF))!=0) return false;
    }
    return true;
}


static bool TestTestImmediateForms() {
    constexpr std::uint64_t CF=1ULL, PF=1ULL<<2, AF=1ULL<<4, ZF=1ULL<<6, SF=1ULL<<7, OF=1ULL<<11;
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(CF|OF|AF);
        std::vector<std::uint8_t> code={0xB0,0x00,0xA8,0x00};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFU)!=0) return false;
        if((cpu.Rflags()&(CF|OF|AF|ZF|PF))!=(ZF|PF)) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0x66,0xB8,0x00,0x80,0x66,0xA9,0x00,0x80};
        if(!Run(m,cpu,code) || (cpu.Rax()&0xFFFFU)!=0x8000U) return false;
        if((cpu.Rflags()&(SF|ZF))!=SF || (cpu.Rflags()&PF)!=PF) return false;
        if((cpu.Rflags()&(CF|OF|AF))!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code={0xB8,0x00,0x00,0x00,0x80,0xA9,0x00,0x00,0x00,0x80};
        if(!Run(m,cpu,code) || cpu.Rax()!=0x0000000080000000ULL) return false;
        if((cpu.Rflags()&SF)==0 || (cpu.Rflags()&ZF)!=0 || (cpu.Rflags()&CF)!=0 ||
           (cpu.Rflags()&OF)!=0 || (cpu.Rflags()&AF)!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFFF00000000ULL);
        code.insert(code.end(),{0x48,0xA9,0x00,0x00,0x00,0x80});
        if(!Run(m,cpu,code) || cpu.Rax()!=0xFFFFFFFF00000000ULL) return false;
        if((cpu.Rflags()&SF)==0 || (cpu.Rflags()&ZF)!=0) return false;
    }
    {
        Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x000000007FFFFFFFULL);
        code.insert(code.end(),{0x48,0xA9,0x00,0x00,0x00,0x80});
        if(!Run(m,cpu,code) || cpu.Rax()!=0x000000007FFFFFFFULL) return false;
        if((cpu.Rflags()&(ZF|SF))!=ZF) return false;
    }
    return true;
}

static bool TestSetccExtendedMemoryAndFlags() {
    constexpr std::uint64_t CF = 1ULL;
    constexpr std::uint64_t PF = 1ULL << 2;
    constexpr std::uint64_t AF = 1ULL << 4;
    constexpr std::uint64_t ZF = 1ULL << 6;
    constexpr std::uint64_t SF = 1ULL << 7;
    constexpr std::uint64_t OF = 1ULL << 11;

    Memory m; m.Map(0x1000, 0x3000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code, 11, 0x1800);
    cpu.SetRflags(CF | PF | AF | ZF | SF | OF);

    // REX.B memory destinations must address R11 and SETcc must preserve flags.
    code.insert(code.end(), {0x41, 0x0F, 0x92, 0x43, 0x00}); // SETC [R11]
    code.insert(code.end(), {0x41, 0x0F, 0x94, 0x43, 0x01}); // SETZ [R11+1]
    code.insert(code.end(), {0x41, 0x0F, 0x97, 0x43, 0x02}); // SETA [R11+2]
    code.insert(code.end(), {0x41, 0x0F, 0x9A, 0x43, 0x03}); // SETP [R11+3]

    if (!Run(m, cpu, code)) return false;
    std::uint8_t values[4] = {};
    if (!m.Read(0x1800, values, sizeof(values))) return false;
    if (values[0] != 1 || values[1] != 1 || values[2] != 0 || values[3] != 1) return false;
    return cpu.Rflags() == (CF | PF | AF | ZF | SF | OF);
}

static bool TestJccConditionMatrix() {
    // Exercise every Jcc condition code with both short and near encodings.
    // Three flag states cover CF/ZF/PF/SF/OF combinations, including SF != OF.
    const std::uint64_t CF = 1ULL;
    const std::uint64_t PF = 1ULL << 2;
    const std::uint64_t ZF = 1ULL << 6;
    const std::uint64_t SF = 1ULL << 7;
    const std::uint64_t OF = 1ULL << 11;

    const std::array<std::uint8_t, 16> shortOpcodes = {
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
        0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F
    };

    const std::array<std::uint64_t, 3> flagStates = {
        0,
        CF | PF | ZF | SF | OF,
        SF
    };

    for (const auto flags : flagStates) {
        for (std::size_t cc = 0; cc < shortOpcodes.size(); ++cc) {
            const bool expected =
                (cc == 0) ? ((flags & OF) != 0) :
                (cc == 1) ? ((flags & OF) == 0) :
                (cc == 2) ? ((flags & CF) != 0) :
                (cc == 3) ? ((flags & CF) == 0) :
                (cc == 4) ? ((flags & ZF) != 0) :
                (cc == 5) ? ((flags & ZF) == 0) :
                (cc == 6) ? ((flags & (CF | ZF)) != 0) :
                (cc == 7) ? ((flags & (CF | ZF)) == 0) :
                (cc == 8) ? ((flags & SF) != 0) :
                (cc == 9) ? ((flags & SF) == 0) :
                (cc == 10) ? ((flags & PF) != 0) :
                (cc == 11) ? ((flags & PF) == 0) :
                (cc == 12) ? (((flags & SF) != 0) != ((flags & OF) != 0)) :
                (cc == 13) ? (((flags & SF) != 0) == ((flags & OF) != 0)) :
                (cc == 14) ? (((flags & ZF) != 0) || (((flags & SF) != 0) != ((flags & OF) != 0))) :
                ((flags & ZF) == 0) && (((flags & SF) != 0) == ((flags & OF) != 0));

            Memory m; m.Map(0x1000, 0x2000); Cpu cpu; cpu.ConnectMemory(&m);
            cpu.SetRflags(flags);
            const std::vector<std::uint8_t> code = {
                shortOpcodes[cc], 0x03, 0xB0, 0x01, 0xF4, 0xB0,
                0x02, 0xF4
            };
            if (!Run(m, cpu, code) || (cpu.Rax() & 0xFFU) != (expected ? 0x02U : 0x01U)) {
                return false;
            }
            if (cpu.Rflags() != flags) return false;

            Memory mn; mn.Map(0x1000, 0x2000); Cpu cn; cn.ConnectMemory(&mn);
            cn.SetRflags(flags);
            const std::vector<std::uint8_t> nearCode = {
                0x0F, static_cast<std::uint8_t>(0x80 + cc),
                0x03, 0x00, 0x00, 0x00, 0xB0, 0x01, 0xF4, 0xB0,
                0x02, 0xF4
            };
            if (!Run(mn, cn, nearCode) || (cn.Rax() & 0xFFU) != (expected ? 0x02U : 0x01U)) {
                return false;
            }
            if (cn.Rflags() != flags) return false;
        }
    }

    return true;
}

static bool TestDivisionSignedAndExtendedForms() {
    // IDIV64 register: (-10) / 3 = -3 remainder -1.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFFFFFFFFFF6ULL);
        AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,10,3);
        code.insert(code.end(),{0x49,0xF7,0xFA}); // IDIV R10
        if(!Run(m,cpu,code) ||
           cpu.Rax()!=0xFFFFFFFFFFFFFFFDULL ||
           cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL) return false;
    }

    // IDIV64 memory with REX.B + disp8 addressing.
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::int64_t divisor=3;
        if(!m.Write(0x1808,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFFFFFFFFFF6ULL);
        AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,11,0x1800);
        code.insert(code.end(),{0x49,0xF7,0x7B,0x08}); // IDIV qword [R11+8]
        if(!Run(m,cpu,code) ||
           cpu.Rax()!=0xFFFFFFFFFFFFFFFDULL ||
           cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL) return false;
    }

    // IDIV64 memory with REX.X/B SIB addressing: [R11 + R12*4 + 8].
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::int64_t divisor=3;
        if(!m.Write(0x1810,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFFFFFFFFFF6ULL);
        AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4F,0xF7,0x7C,0xA3,0x08}); // IDIV qword [R11+R12*4+8]
        if(!Run(m,cpu,code) ||
           cpu.Rax()!=0xFFFFFFFFFFFFFFFDULL ||
           cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL) return false;
    }

    // IDIV32 negative dividend: EDX:EAX = -10, divisor 3.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFF6ULL);
        AppendMovR64(code,2,0xFFFFFFFFULL);
        AppendMovR64(code,10,3);
        code.insert(code.end(),{0x41,0xF7,0xFA}); // IDIV R10D
        if(!Run(m,cpu,code) ||
           static_cast<std::uint32_t>(cpu.Rax())!=0xFFFFFFFDULL ||
           static_cast<std::uint32_t>(cpu.ReadRegister64(2))!=0xFFFFFFFFU) return false;
    }

    // IDIV16 negative dividend: DX:AX = -10, divisor 3.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFF6ULL);
        AppendMovR64(code,2,0xFFFFULL);
        AppendMovR64(code,10,3);
        code.insert(code.end(),{0x66,0x41,0xF7,0xFA}); // IDIV R10W
        if(!Run(m,cpu,code) ||
           (cpu.Rax()&0xFFFFU)!=0xFFFDU ||
           (cpu.ReadRegister64(2)&0xFFFFU)!=0xFFFFU) return false;
    }

    // IDIV8 negative AX dividend: -10 / 3 = -3, remainder -1.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFF6ULL);
        AppendMovR64(code,10,3);
        code.insert(code.end(),{0x41,0xF6,0xFA}); // IDIV R10B
        if(!Run(m,cpu,code) ||
           (cpu.Rax()&0xFFU)!=0xFDU ||
           ((cpu.Rax()>>8)&0xFFU)!=0xFFU) return false;
    }

    // 64-bit quotient overflow must raise #DE and leave destination registers unchanged.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        bool seen=false;
        cpu.SetExceptionHandler([&](Cpu&, const CpuException& e) {
            seen = e.vector == CpuExceptionVector::DivideError;
            return true;
        });
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0);
        AppendMovR64(code,2,1);
        AppendMovR64(code,10,1);
        code.insert(code.end(),{0x49,0xF7,0xF2}); // DIV R10; RDX >= divisor => overflow
        if(!m.Write(0x1000,code.data(),code.size())) return false;
        cpu.SetInstructionPointer(0x1000);
        if(cpu.Run()==0 || !seen || cpu.Rax()!=0 || cpu.ReadRegister64(2)!=1) return false;
    }

    return true;
}


static bool TestDivisionUnsignedAndQuotientBoundaries() {
    // DIV64 register: RDX:RAX = 0x0:1 / 2 -> quotient 0, remainder 1.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,1);
        AppendMovR64(code,2,0);
        AppendMovR64(code,10,2);
        code.insert(code.end(),{0x49,0xF7,0xF2}); // DIV R10
        if(!Run(m,cpu,code) || cpu.Rax()!=0 ||
           cpu.ReadRegister64(2)!=1) return false;
    }

    // DIV64 memory with REX.X/B SIB: 2^60 / 2 -> 2^59, exact remainder 0.
    {
        Memory m; m.Map(0x1000,0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t divisor=2;
        if(!m.Write(0x1810,reinterpret_cast<const std::uint8_t*>(&divisor),sizeof(divisor))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0x1000000000000000ULL);
        AppendMovR64(code,2,0);
        AppendMovR64(code,11,0x1800);
        AppendMovR64(code,12,2);
        code.insert(code.end(),{0x4F,0xF7,0x74,0xA3,0x08}); // DIV qword [R11+R12*4+8]
        if(!Run(m,cpu,code) ||
           cpu.Rax()!=0x0800000000000000ULL ||
           cpu.ReadRegister64(2)!=0) return false;
    }

    // IDIV64 register: -1 / -2 -> quotient 0, remainder -1.
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code,0,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,2,0xFFFFFFFFFFFFFFFFULL);
        AppendMovR64(code,10,0xFFFFFFFFFFFFFFFEULL);
        code.insert(code.end(),{0x49,0xF7,0xFA}); // IDIV R10
        if(!Run(m,cpu,code) ||
           cpu.Rax()!=0 ||
           cpu.ReadRegister64(2)!=0xFFFFFFFFFFFFFFFFULL) return false;
    }

    return true;
}

static bool TestAdcSbbQwordMemoryBoundaries() {
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint64_t value=0x7FFFFFFFFFFFFFFFULL;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        cpu.SetRflags(cpu.Rflags() | 1ULL);
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x81,0x17,0x00,0x00,0x00,0x00}); // ADC qword [RDI],0
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) ||
           out!=0x8000000000000000ULL) return false;
        const auto f=cpu.Rflags();
        if((f&(1ULL|(1ULL<<4)|(1ULL<<7)|(1ULL<<11)))!=((1ULL<<4)|(1ULL<<7)|(1ULL<<11))) return false;
    }
    {
        Memory m; m.Map(0x1000,0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::uint64_t value=0;
        if(!m.Write(0x1800,reinterpret_cast<const std::uint8_t*>(&value),sizeof(value))) return false;
        cpu.SetRflags(cpu.Rflags() | 1ULL);
        std::vector<std::uint8_t> code; AppendMovR64(code,7,0x1800);
        code.insert(code.end(),{0x48,0x81,0x1F,0x00,0x00,0x00,0x00}); // SBB qword [RDI],0
        if(!Run(m,cpu,code)) return false;
        std::uint64_t out=0;
        if(!m.Read(0x1800,reinterpret_cast<std::uint8_t*>(&out),sizeof(out)) ||
           out!=0xFFFFFFFFFFFFFFFFULL) return false;
        const auto f=cpu.Rflags();
        if((f&(1ULL|(1ULL<<2)|(1ULL<<4)|(1ULL<<7)))!=(1ULL|(1ULL<<2)|(1ULL<<4)|(1ULL<<7))) return false;
        if((f&((1ULL<<6)|(1ULL<<11)))!=0) return false;
    }
    return true;
}


static bool TestRexLowByteAliases() {
    Memory m; m.Map(0x1000,0x2000); Cpu cpu; cpu.ConnectMemory(&m);
    std::vector<std::uint8_t> code;
    AppendMovR64(code,4,0x1122334455667788ULL);
    code.push_back(0xB0); code.push_back(0x5A);
    code.insert(code.end(),{0x40,0x88,0xC4});
    code.insert(code.end(),{0x40,0x8A,0xC4});
    if(!Run(m,cpu,code)) return false;
    if(cpu.ReadRegister64(4)!=0x112233445566775AULL) return false;
    return (cpu.Rax()&0xFFU)==0x5AU;
}


static bool TestGroup1ImmediateExtendedCoverage() {
    const std::uint64_t CF = 1ULL;
    const std::uint64_t ZF = 1ULL << 6;
    const std::uint64_t SF = 1ULL << 7;
    const std::uint64_t OF = 1ULL << 11;
    const std::uint64_t AF = 1ULL << 4;
    const std::uint64_t PF = 1ULL << 2;

    // 64-bit 81 /0: ADD with sign-extended imm32 and REX.W.
    {
        Memory m; m.Map(0x1000, 0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0x0000000080000000ULL);
        code.insert(code.end(), {0x48, 0x81, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF});
        if (!Run(m, cpu, code) || cpu.Rax() != 0x000000007FFFFFFFULL) return false;
        if ((cpu.Rflags() & (ZF | SF | OF)) != 0 || (cpu.Rflags() & CF) == 0) return false;
    }

    // 64-bit 81 /5: SUB with a negative imm32 (sign extension matters).
    {
        Memory m; m.Map(0x1000, 0x3000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0);
        code.insert(code.end(), {0x48, 0x81, 0xE8, 0x00, 0x00, 0x00, 0x80});
        if (!Run(m, cpu, code) || cpu.Rax() != 0x0000000080000000ULL) return false;
        if ((cpu.Rflags() & (CF | SF | OF)) != CF) return false;
    }

    // 64-bit 81 /7: CMP must consume the sign-extended imm32 without modifying RAX.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 0, 0x000000007FFFFFFFULL);
        code.insert(code.end(), {0x48, 0x81, 0xF8, 0x00, 0x00, 0x00, 0x80});
        if (!Run(m, cpu, code) || cpu.Rax() != 0x000000007FFFFFFFULL) return false;
        if ((cpu.Rflags() & (CF | ZF | SF | OF)) != CF) return false;
    }

    // 16-bit 83 /0 and /5: immediate is sign-extended to operand width.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code = {
            0x66, 0xB8, 0x00, 0x00,
            0x66, 0x83, 0xC0, 0xFF,
            0x66, 0x83, 0xE8, 0x01
        };
        if (!Run(m, cpu, code) || (cpu.Rax() & 0xFFFFU) != 0xFFFEU) return false;
        if ((cpu.Rflags() & (CF | SF)) != SF) return false;
    }

    // 32-bit 83 /6: XOR must zero-extend the destination register.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        std::vector<std::uint8_t> code = {
            0x48, 0xB8, 0xFFFF0000000000FFULL & 0xFF,
        };
        code.clear();
        AppendMovR64(code, 0, 0x12340000000000FFULL);
        code.insert(code.end(), {0x83, 0xF0, 0xFF});
        if (!Run(m, cpu, code) || cpu.Rax() != 0x00000000FFFFFF00ULL) return false;
    }

    // 8-bit 80 /2: ADC with CF and AF/PF boundary behavior.
    {
        Memory m; m.Map(0x1000, 0x2000); Cpu cpu; cpu.ConnectMemory(&m);
        cpu.SetRflags(cpu.Rflags() | CF);
        std::vector<std::uint8_t> code = {0xB0, 0x7F, 0x80, 0xD0, 0x00};
        if (!Run(m, cpu, code) || (cpu.Rax() & 0xFFU) != 0x80U) return false;
        const auto f = cpu.Rflags();
        if ((f & (AF | SF | OF)) != (AF | SF | OF) || (f & ZF) != 0) return false;
        if ((f & PF) != 0) return false;
    }

    // 8-bit 80 /2 ADC memory with REX.X/B SIB addressing and carry-in.
    {
        Memory m; m.Map(0x1000, 0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint8_t value = 0x7F;
        if (!m.Write(0x1A01, &value, 1)) return false;
        cpu.SetRflags(CF);
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 11, 0x1800);
        AppendMovR64(code, 12, 0x80);
        code.insert(code.end(), {0x4F, 0x80, 0x54, 0xA3, 0x01, 0x7F});
        if (!Run(m, cpu, code)) return false;
        std::uint8_t out = 0;
        if (!m.Read(0x1A01, &out, 1) || out != 0xFFU) return false;
        const auto f = cpu.Rflags();
        if ((f & (AF | OF | PF)) != (AF | OF | PF)) return false;
        if ((f & (CF | ZF)) != 0) return false;
    }

    // REX.B + SIB memory form: 83 /6 XOR byte/word-independent addressing path is
    // exercised here at dword width with an extended base and extended index.
    {
        Memory m; m.Map(0x1000, 0x4000); Cpu cpu; cpu.ConnectMemory(&m);
        const std::uint32_t value = 0xFFFFFFFFU;
        if (!m.Write(0x1A00, reinterpret_cast<const std::uint8_t*>(&value), sizeof(value))) return false;
        std::vector<std::uint8_t> code;
        AppendMovR64(code, 11, 0x1800);
        AppendMovR64(code, 12, 0x80);
        code.insert(code.end(), {0x47, 0x83, 0x34, 0xA3, 0xFF});
        if (!Run(m, cpu, code)) return false;
        std::uint32_t out = 0;
        if (!m.Read(0x1A00, reinterpret_cast<std::uint8_t*>(&out), sizeof(out))) return false;
        return out == 0x00000000U;
    }
}

static bool TestMovExtendExtendedForms() {
    Memory m; m.Map(0x1000, 0x4000); Cpu cpu; cpu.ConnectMemory(&m);

    const std::uint16_t wordA = 0x80FF;
    const std::uint16_t wordB = 0x7F01;
    const std::uint8_t byteA = 0xFE;
    const std::uint8_t byteB = 0x80;
    if (!m.Write(0x1A02, reinterpret_cast<const std::uint8_t*>(&wordA), sizeof(wordA)) ||
        !m.Write(0x1A04, reinterpret_cast<const std::uint8_t*>(&wordB), sizeof(wordB)) ||
        !m.Write(0x1A06, &byteA, 1) ||
        !m.Write(0x1A07, &byteB, 1)) return false;

    std::vector<std::uint8_t> code;
    AppendMovR64(code, 11, 0x1800);
    AppendMovR64(code, 12, 0x80);
    code.insert(code.end(), {0x4F, 0x0F, 0xB7, 0x4C, 0xA3, 0x02});
    code.insert(code.end(), {0x4F, 0x0F, 0xBF, 0x54, 0xA3, 0x04});
    code.insert(code.end(), {0x47, 0x0F, 0xB6, 0x44, 0xA3, 0x06});
    code.insert(code.end(), {0x47, 0x0F, 0xBE, 0x4C, 0xA3, 0x07});

    if (!Run(m, cpu, code)) return false;
    if (cpu.ReadRegister64(9) != 0x00000000FFFFFF80ULL) return false;
    if (cpu.ReadRegister64(10) != 0x0000000000007F01ULL) return false;
    return cpu.ReadRegister64(8) == 0x00000000000000FEULL;
}


int main() {
    if (!TestTestImmediateForms()) { std::cerr << "TEST immediate forms failed\n"; return 43; }
    if (!TestSetccExtendedMemoryAndFlags()) { std::cerr << "SETcc extended memory/flags failed\n"; return 44; }
    if (!TestCmov16ExtendedMemoryAndFlags()) { std::cerr << "CMOV16 extended memory/flags failed\n"; return 45; }
    if (!TestGroup1ImmediateExtendedCoverage()) { std::cerr << "Group1 immediate extended coverage failed\n"; return 99; }
    if (!TestMovsxd()) { std::cerr << "MOVSXD failed\n"; return 1; }
    if (!TestMovExtendExtendedForms()) { std::cerr << "MOVZX/MOVSX extended forms failed\n"; return 47; }
    if (!TestBswap()) { std::cerr << "BSWAP failed\n"; return 2; }
    if (!TestCmovz()) { std::cerr << "CMOVZ failed\n"; return 3; }
    if (!TestCmovccExtendedConditions()) { std::cerr << "CMOVcc extended conditions failed\n"; return 43; }
    if (!TestXadd32()) { std::cerr << "XADD failed\n"; return 4; }
    if (!TestXadd8()) { std::cerr << "XADD8 failed\n"; return 5; }
    if (!TestCmpxchg64()) { std::cerr << "CMPXCHG failed\n"; return 6; }
    if (!TestMultiByteNop()) { std::cerr << "multi-byte NOP failed\n"; return 6; }
    if (!TestCmpxchg8b()) { std::cerr << "CMPXCHG8B failed\n"; return 6; }
    if (!TestSystemIntegerOps()) { std::cerr << "system integer ops failed\n"; return 6; }
    if (!TestLockPrefix()) { std::cerr << "LOCK prefix failed\n"; return 6; }
    if (!TestPopRm()) { std::cerr << "POP r/m failed\n"; return 6; }
    if (!TestDoubleShift()) { std::cerr << "double shift failed\n"; return 6; }
    if (!TestDoubleShiftExtendedForms()) { std::cerr << "double shift extended forms failed\n"; return 35; }
    if (!TestBitModify()) { std::cerr << "bit modify failed\n"; return 6; }
    if (!TestBitMemoryForms()) { std::cerr << "bit memory forms failed\n"; return 36; }
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
    if (!TestGroup1RexExtendedRegisters()) { std::cerr << "Group-1 REX extended registers failed\n"; return 8; }
    if (!TestAdcSbb16Directions()) { std::cerr << "ADC/SBB 16-bit directions failed\n"; return 8; }
    if (!TestAdcSbbImmediateAndWidths()) { std::cerr << "ADC/SBB immediate and widths failed\n"; return 8; }
    if (!TestIncDecMemoryAndCmpWidths()) { std::cerr << "INC/DEC memory and CMP widths failed\n"; return 8; }
    if (!TestNegWidths()) { std::cerr << "NEG widths failed\n"; return 9; }
    if (!TestImulForms()) { std::cerr << "IMUL forms failed\n"; return 10; }
    if (!TestMulDivForms()) { std::cerr << "MUL/DIV forms failed\n"; return 11; }
    if (!TestOneOperandMulWidths()) { std::cerr << "one-operand MUL/IMUL failed\n"; return 12; }
    if (!TestCmpByteForms()) { std::cerr << "byte CMP forms failed\n"; return 14; }
    if (!TestDivideFaultsAndBoundaries()) { std::cerr << "DIV/IDIV faults failed\n"; return 13; }
    if (!TestCmpImmediateForms()) { std::cerr << "immediate CMP forms failed\n"; return 16; }
    if (!TestGroup1ImmediateWidths()) { std::cerr << "Group1 immediate widths failed\\n"; return 17; }
    if (!TestGroup1FullImmediateForms()) { std::cerr << "Group1 full immediate forms failed\n"; return 20; }
    if (!TestGroup1RexAndMemory()) { std::cerr << "Group1 REX/memory failed\n"; return 19; }
    if (!TestGroup1ExtendedAddressing()) { std::cerr << "Group1 extended addressing failed\n"; return 21; }
    if (!TestGroup1ExtendedAllWidths()) { std::cerr << "Group1 extended all widths failed\n"; return 22; }
    if (!TestGroup1QwordImmediateMemory()) { std::cerr << "Group1 qword immediate memory failed\n"; return 37; }
    if (!TestTestRmRegForms()) { std::cerr << "TEST r/m,r forms failed\n"; return 38; }
    if (!TestGroup1FlagMatrix()) { std::cerr << "Group1 flag matrix failed\n"; return 18; }
    if (!TestCmpUnequalFlags()) { std::cerr << "unequal CMP flags failed\n"; return 15; }
    if (!TestShiftLeft64Parity()) { std::cerr << "64-bit SHL parity failed\n"; return 26; }
    if (!TestShift16ExtendedRegister()) { std::cerr << "16-bit shift extended register failed\n"; return 34; }
    if (!TestShift32Parity()) { std::cerr << "32-bit shift parity failed\n"; return 30; }
    if (!TestShift32ZeroCount()) { std::cerr << "32-bit shift zero-count failed\n"; return 33; }
    if (!TestShiftLeft64CLFlags()) { std::cerr << "64-bit SHL CL flags failed\n"; return 27; }
    if (!TestShiftRight64Forms()) { std::cerr << "64-bit SHR/SAR forms failed\n"; return 23; }
    if (!TestLeaExtendedAddressing()) { std::cerr << "LEA extended addressing failed\n"; return 24; }
    if (!TestLeaExtendedNoBase()) { std::cerr << "LEA extended no-base SIB failed\n"; return 29; }
    if (!TestRotate32ExtendedForms()) { std::cerr << "32-bit rotate extended forms failed\n"; return 32; }
    if (!TestRotate32ZeroCount()) { std::cerr << "32-bit rotate zero-count failed\n"; return 31; }
    if (!TestRotate64ZeroCount()) { std::cerr << "64-bit rotate zero-count failed\n"; return 28; }
    if (!TestRotate64Forms()) { std::cerr << "64-bit rotate forms failed\n"; return 25; }
    if (!TestCpuid()) { std::cerr << "CPUID failed\n"; return 8; }
    if (!TestRexLowByteAliases()) { std::cerr << "REX low-byte aliases failed\n"; return 41; }
    if (!TestAdcSbbQwordMemoryBoundaries()) { std::cerr << "ADC/SBB qword memory boundaries failed\n"; return 40; }
    if (!TestJccConditionMatrix()) { std::cerr << "Jcc condition matrix failed\\n"; return 46; }
    if (!TestDivisionSignedAndExtendedForms()) { std::cerr << "signed/extended division forms failed\\n"; return 39; }
    if (!TestDivisionUnsignedAndQuotientBoundaries()) { std::cerr << "unsigned/quotient division boundaries failed\n"; return 42; }
    std::cout << "x86 extended integer instruction test: PASS\n";
    return 0;
}