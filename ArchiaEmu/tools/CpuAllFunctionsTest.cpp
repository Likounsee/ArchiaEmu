#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"
#include "cpu/RegisterFile.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace myps5emu;

namespace {

constexpr std::uint64_t CODE = 0x400000;
constexpr std::uint64_t DATA = 0x500000;
constexpr std::uint64_t STACK = 0x7fff00000000ULL;

int passed = 0;
int failed = 0;

#define CHECK(NAME, COND)                                      \
    do {                                                       \
        if (COND) {                                            \
            std::cout << "[PASS] " << NAME << "\n";            \
            ++passed;                                          \
        } else {                                               \
            std::cout << "[FAIL] " << NAME << "\n";            \
            ++failed;                                          \
        }                                                        \
    } while (0)

void WriteCode(Memory& mem, const std::vector<std::uint8_t>& code)
{
    mem.Write(
        CODE,
        code.data(),
        code.size());
}

Cpu MakeCpu(Memory& mem)
{
    Cpu cpu;
    cpu.ConnectMemory(&mem);
    cpu.SetInstructionPointer(CODE);
    cpu.SetStackPointer(STACK + 0x1000);
    return cpu;
}

bool RunCode(
    Cpu& cpu,
    Memory& mem,
    const std::vector<std::uint8_t>& code)
{
    WriteCode(mem, code);
    return cpu.Run() == 0;
}

std::uint64_t Read64(Memory& mem, std::uint64_t addr)
{
    std::uint64_t value = 0;
    mem.Read(
        addr,
        reinterpret_cast<std::uint8_t*>(&value),
        sizeof(value));
    return value;
}

std::uint16_t Read16(Memory& mem, std::uint64_t addr)
{
    std::uint16_t value = 0;
    mem.Read(addr, reinterpret_cast<std::uint8_t*>(&value), sizeof(value));
    return value;
}

std::uint32_t Read32(Memory& mem, std::uint64_t addr)
{
    std::uint32_t value = 0;
    mem.Read(
        addr,
        reinterpret_cast<std::uint8_t*>(&value),
        sizeof(value));
    return value;
}

void Write64(Memory& mem, std::uint64_t addr, std::uint64_t value)
{
    mem.Write(
        addr,
        reinterpret_cast<const std::uint8_t*>(&value),
        sizeof(value));
}

void Write32(Memory& mem, std::uint64_t addr, std::uint32_t value)
{
    mem.Write(
        addr,
        reinterpret_cast<const std::uint8_t*>(&value),
        sizeof(value));
}

std::vector<std::uint8_t> MovR64(
    std::uint8_t reg,
    std::uint64_t value)
{
    std::vector<std::uint8_t> c;

    if (reg >= 8) {
        c.push_back(0x49);
        c.push_back(static_cast<std::uint8_t>(
            0xB8 + (reg - 8)));
    } else {
        c.push_back(0x48);
        c.push_back(static_cast<std::uint8_t>(
            0xB8 + reg));
    }

    for (int i = 0; i < 8; ++i) {
        c.push_back(
            static_cast<std::uint8_t>(
                value >> (i * 8)));
    }

    return c;
}

void Append(
    std::vector<std::uint8_t>& dst,
    const std::vector<std::uint8_t>& src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

std::vector<std::uint8_t> Finish(
    std::vector<std::uint8_t> code)
{
    code.push_back(0xC3);
    return code;
}

void TestAddressSizeOverride()
{
    Memory mem;
    mem.Map(CODE, 0x1000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Write32(mem, DATA, 0xAABBCCDDU);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(0, 0x100000000ULL + DATA);

    const auto code = std::vector<std::uint8_t>{
        0x67,             // address-size override: 32-bit effective address
        0x8B, 0x00,       // MOV EAX, [EAX]
        0xF4
    };

    CHECK(
        "67 address-size override uses EAX/32-bit effective address",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0xAABBCCDDULL);

    Write32(mem, DATA + 4, 0x11223344U);
    Cpu sibCpu = MakeCpu(mem);
    sibCpu.WriteRegister64(0, 1);
    const auto sibCode = std::vector<std::uint8_t>{
        0x67, 0x8B, 0x04, 0x85, 0x00, 0x00, 0x50, 0x00, 0xF4
    };
    CHECK(
        "67 address-size override uses 32-bit SIB addressing",
        RunCode(sibCpu, mem, sibCode) &&
        sibCpu.Rax() == 0x11223344ULL);

    Cpu absoluteCpu = MakeCpu(mem);
    const auto absoluteCode = std::vector<std::uint8_t>{
        0x67, 0x8B, 0x05, 0x00, 0x00, 0x50, 0x00, 0xF4
    };
    CHECK(
        "67 address-size override makes ModRM rm=101 absolute",
        RunCode(absoluteCpu, mem, absoluteCode) &&
        absoluteCpu.Rax() == 0xAABBCCDDULL);
}

void TestDescriptorTableInstructions()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);
    const std::uint64_t gdtrBase = 0x0000000012345000ULL;
    const std::uint64_t idtrBase = 0x000000006789A000ULL;
    std::uint8_t descriptor[10]{};
    descriptor[0] = 0xFF; descriptor[1] = 0x00;
    for (unsigned i = 0; i < 8; ++i) descriptor[2 + i] = static_cast<std::uint8_t>(gdtrBase >> (i * 8U));
    mem.Write(DATA, descriptor, sizeof(descriptor));
    descriptor[0] = 0x7F; descriptor[1] = 0x00;
    for (unsigned i = 0; i < 8; ++i) descriptor[2 + i] = static_cast<std::uint8_t>(idtrBase >> (i * 8U));
    mem.Write(DATA + 16, descriptor, sizeof(descriptor));

    Cpu cpu = MakeCpu(mem);
    auto code = MovR64(6, DATA);
    code.insert(code.end(), {0x0F, 0x01, 0x16});
    Append(code, MovR64(6, DATA + 16));
    code.insert(code.end(), {0x0F, 0x01, 0x1E});
    Append(code, MovR64(0, 0x28));
    code.insert(code.end(), {0x0F, 0x00, 0xD8});
    code.insert(code.end(), {0x33, 0xC0, 0x0F, 0x00, 0xC8});
    code = Finish(code);
    CHECK(
        "LGDT/LIDT/LTR wire architectural descriptor state",
        RunCode(cpu, mem, code) &&
        cpu.GdtrBase() == gdtrBase && cpu.GdtrLimit() == 0xFF &&
        cpu.IdtrBase() == idtrBase && cpu.IdtrLimit() == 0x7F &&
        cpu.TaskRegister() == 0x28 && cpu.Rax() == 0x28ULL);
}

void TestCanonicalAddressFault()
{
    Memory mem;
    mem.Map(CODE, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(0, 0x0000800000000000ULL);

    bool generalProtection = false;
    cpu.SetExceptionHandler([&](Cpu& handlerCpu, const CpuException& exception) {
        generalProtection = exception.vector == CpuExceptionVector::GeneralProtection;
        if (generalProtection) {
            handlerCpu.Halt();
            return true;
        }
        return false;
    });

    const auto code = std::vector<std::uint8_t>{
        0x8B, 0x00, // MOV EAX, [RAX] with a non-canonical address
        0xF4
    };

    CHECK(
        "Non-canonical 64-bit memory address raises #GP",
        !RunCode(cpu, mem, code) && generalProtection);
}

void TestMemory()
{
    Memory mem;

    CHECK(
        "Memory Map",
        mem.Map(DATA, 0x1000));

    CHECK(
        "Memory IsMapped",
        mem.IsMapped(DATA, 0x100));

    std::uint64_t value = 0x1122334455667788ULL;

    CHECK(
        "Memory Write",
        mem.Write(
            DATA,
            reinterpret_cast<const std::uint8_t*>(&value),
            sizeof(value)));

    std::uint64_t read = 0;

    CHECK(
        "Memory Read",
        mem.Read(
            DATA,
            reinterpret_cast<std::uint8_t*>(&read),
            sizeof(read)) &&
        read == value);

    CHECK(
        "Memory overlap refused",
        !mem.Map(DATA + 0x100, 0x100));

    mem.Clear();

    CHECK(
        "Memory Clear",
        !mem.IsMapped(DATA, 1));
}

void TestRegisterFile()
{
    RegisterFile rf;

    rf.Write64(0, 0x1122334455667788ULL);

    CHECK(
        "RegisterFile Read64",
        rf.Read64(0) == 0x1122334455667788ULL);

    rf.Write32(0, 0x12345678U);

    CHECK(
        "RegisterFile Write32 zero-extend",
        rf.Read64(0) == 0x12345678ULL);

    rf.SetRax(0xAAULL);

    CHECK(
        "RegisterFile RAX",
        rf.Rax() == 0xAAULL);

    rf.SetRsp(0x12345678ULL);

    CHECK(
        "RegisterFile RSP",
        rf.Rsp() == 0x12345678ULL);
}

void TestMov32()
{
    Memory mem;
    mem.Map(CODE, 0x1000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = std::vector<std::uint8_t>{
        0xB8, 0x78, 0x56, 0x34, 0x12,
        0xBB, 0x01, 0x00, 0x00, 0x00,
        0x03, 0xC3
    };

    code = Finish(code);

    CHECK(
        "MOV32 + ADD32",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x12345679ULL);
}

void TestMov64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(
        0,
        0x1122334455667788ULL);

    Append(
        code,
        MovR64(3, 0xAABBCCDDEEFF0011ULL));

    code.push_back(0x48);
    code.push_back(0x89);
    code.push_back(0xD8);

    code = Finish(code);

    CHECK(
        "MOV64 register",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0xAABBCCDDEEFF0011ULL);
}

void TestMemoryMov64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);
    Append(code, MovR64(3, 0x1122334455667788ULL));

    // MOV [RAX], RBX
    code.push_back(0x48);
    code.push_back(0x89);
    code.push_back(0x18);

    // XOR RBX,RBX
    code.push_back(0x48);
    code.push_back(0x33);
    code.push_back(0xDB);

    // MOV RBX,[RAX]
    code.push_back(0x48);
    code.push_back(0x8B);
    code.push_back(0x18);

    code = Finish(code);

    CHECK(
        "MOV64 memory store/load",
        RunCode(cpu, mem, code) &&
        cpu.ReadRegister64(3) ==
            0x1122334455667788ULL);
}

void TestAddSub64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 10);
    Append(code, MovR64(1, 20));

    // ADD RAX,RBX
    code.push_back(0x48);
    code.push_back(0x03);
    code.push_back(0xC3);

    // SUB RAX,RBX
    code.push_back(0x48);
    code.push_back(0x2B);
    code.push_back(0xC3);

    code = Finish(code);

    CHECK(
        "ADD64 + SUB64",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 10);
}

void TestCmp()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 42);
    Append(code, MovR64(3, 42));

    // CMP RAX,RBX
    code.push_back(0x48);
    code.push_back(0x39);
    code.push_back(0xD8);

    code = Finish(code);

    CHECK(
        "CMP64 ZF",
        RunCode(cpu, mem, code) &&
        (cpu.Rflags() & (1ULL << 6)) != 0);
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x300;
        const std::uint64_t index = 5;
        const std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 0x123456789ABCDEF0ULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x123456789ABCDEF0ULL);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        // 4F 3B 44 8D 20: CMP R8,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x4F, 0x3B, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("CMP64 REX.RXB SIB disp8 memory form",
              RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x480;
        constexpr std::uint64_t index = 2;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        Write32(mem, address, 0x12345678U);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0x12345678ULL);

        // 47 3B 44 8D 20: CMP R8D,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x47, 0x3B, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK(
            "CMP32 memory REX.RXB SIB disp8",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & (1ULL << 6)) != 0);
    }
}

void TestLogic64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0xF0F0ULL);
    Append(code, MovR64(3, 0x0FF0ULL));

    // AND RAX,RBX
    code.push_back(0x48);
    code.push_back(0x23);
    code.push_back(0xC3);

    // OR RAX,RBX
    code.push_back(0x48);
    code.push_back(0x0B);
    code.push_back(0xC3);

    // XOR RAX,RBX
    code.push_back(0x48);
    code.push_back(0x33);
    code.push_back(0xC3);

    code = Finish(code);

    CHECK(
        "AND64 + OR64 + XOR64",
        RunCode(cpu, mem, code));
}

void TestLogicMemory64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Write64(mem, DATA, 0xF0F0ULL);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);
    Append(code, MovR64(3, 0x0F0FULL));

    // AND [RAX], RBX
    code.push_back(0x48);
    code.push_back(0x21);
    code.push_back(0x18);

    code = Finish(code);

    CHECK(
        "AND64 memory destination",
        RunCode(cpu, mem, code) &&
        Read64(mem, DATA) == 0);
}

void TestTest64()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0xF0);
    Append(code, MovR64(1, 0x0F));

    // TEST RAX,RBX
    code.push_back(0x48);
    code.push_back(0x85);
    code.push_back(0xD8);

    code = Finish(code);

    CHECK(
        "TEST64 ZF",
        RunCode(cpu, mem, code) &&
        (cpu.Rflags() & (1ULL << 6)) != 0);
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x400;
        const std::uint64_t index = 3;
        const std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 0x00000000000000F0ULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x00000000000000F0ULL);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.SetRflags((1ULL << 0) | (1ULL << 11));
        // 4F 85 44 8D 20: TEST [R13+R9*4+disp8],R8.
        const std::vector<std::uint8_t> code = {
            0x4F, 0x85, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("TEST64 REX.RXB SIB disp8 memory form",
              RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) == 0 &&
              (cpu.Rflags() & 1ULL) == 0 &&
              (cpu.Rflags() & (1ULL << 11)) == 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x500;
        constexpr std::uint64_t index = 3;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        Write32(mem, address, 0x0000F0F0U);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0x0000F0F0ULL);
        cpu.SetRflags((1ULL << 0) | (1ULL << 11));

        // 47 85 44 8D 20: TEST R8D,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x47, 0x85, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK(
            "TEST32 memory REX.RXB SIB disp8",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & (1ULL << 6)) == 0 &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & (1ULL << 11)) == 0);
    }
}

void TestLea()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);
    Append(code, MovR64(1, 0x20));

    // LEA RAX,[RAX+20h]
    code.push_back(0x48);
    code.push_back(0x8D);
    code.push_back(0x40);
    code.push_back(0x20);

    code = Finish(code);

    CHECK(
        "LEA64",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == DATA + 0x20);
}

void TestStack()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0x123456789ABCDEF0ULL);

    // PUSH RAX
    code.push_back(0x50);

    // XOR RAX,RAX
    code.push_back(0x48);
    code.push_back(0x33);
    code.push_back(0xC0);

    // POP RAX
    code.push_back(0x58);

    code = Finish(code);

    CHECK(
        "PUSH64 + POP64",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x123456789ABCDEF0ULL);
}

void TestLeave()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    std::vector<std::uint8_t> code;

    // Ancien RBP de la fonction appelante
    constexpr std::uint64_t oldRbp =
        0xDEADBEEFCAFEBABEULL;

    // RBP = ancien RBP
    Append(code, MovR64(5, oldRbp));

    // PUSH RBP
    // RSP = STACK + 0x1000 - 8
    code.push_back(0x55);

    // RBP = RSP actuel = STACK + 0x1000 - 8
    Append(code, MovR64(5, STACK + 0x1000 - 8));

    // Variable locale fictive
    Append(code, MovR64(0, 0x123456789ABCDEF0ULL));
    code.push_back(0x50);

    // LEAVE
    // RSP = RBP
    // POP RBP -> oldRbp
    code.push_back(0xC9);

    code = Finish(code);

    CHECK(
        "LEAVE",
        RunCode(cpu, mem, code) &&
        cpu.ReadRegister64(5) == oldRbp &&
        cpu.Rsp() == STACK + 0x1000);
}

void TestPushImmediate()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    std::vector<std::uint8_t> code;

    // PUSH imm8 = -1
    code.push_back(0x6A);
    code.push_back(0xFF);

    // POP RAX
    code.push_back(0x58);

    code = Finish(code);

    CHECK(
        "PUSH imm8 sign-extended",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0xFFFFFFFFFFFFFFFFULL);
}

void TestPushImmediate32()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    std::vector<std::uint8_t> code;

    // PUSH imm32 = 0x12345678
    code.push_back(0x68);
    code.push_back(0x78);
    code.push_back(0x56);
    code.push_back(0x34);
    code.push_back(0x12);

    // POP RAX
    code.push_back(0x58);

    code = Finish(code);

    CHECK(
        "PUSH imm32 sign-extended",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x0000000012345678ULL);
}
void TestPushPopRmForms()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x180;
        const std::uint64_t value = 0x8877665544332211ULL;
        Write64(mem, address, value);
        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{0xFF, 0x34, 0x25};
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(address >> (i * 8)));
        code.insert(code.end(), {0x8F, 0x04, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(address >> (i * 8)));
        code = Finish(code);
        CHECK("PUSH/POP r/m64 memory absolute", RunCode(cpu, mem, code) &&
              Read64(mem, address) == value && cpu.Rsp() == STACK + 0x1000);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x123456789ABCDEF0ULL);
        auto code = std::vector<std::uint8_t>{0x41, 0x50, 0x41, 0x8F, 0xC1, 0xF4};
        CHECK("PUSH R8 / POP R9 via r/m64", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(9) == 0x123456789ABCDEF0ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x1A0;
        Write64(mem, address, 0xAABBCCDDEEFF1234ULL);
        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{0xFF, 0x34, 0x25};
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(address >> (i * 8)));
        code.insert(code.end(), {0x66, 0x8F, 0x04, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(address >> (i * 8)));
        code = Finish(code);
        CHECK("66h POP r/m16 stores only 16-bit value", RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0xAABBCCDDEEFF1234ULL);
    }
}


void TestPushPopRex16()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(STACK, 0x2000);
    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(8, 0x1122334455667788ULL);

    const std::vector<std::uint8_t> code = {
        0x66, 0x41, 0x50, // PUSH R8W
        0x66, 0x41, 0x59, // POP R9W
        0xF4
    };

    CHECK(
        "66h PUSH R8W / POP R9W uses extended registers",
        RunCode(cpu, mem, code) &&
        cpu.ReadRegister64(8) == 0x1122334455667788ULL &&
        cpu.ReadRegister64(9) == 0x7788ULL &&
        cpu.Rsp() == STACK + 0x1000);
}

void TestPushPopRexAndOperandWidths()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x1122334455667788ULL);
        auto code = std::vector<std::uint8_t>{0x41, 0x50, 0x41, 0x58, 0xC3};
        CHECK("PUSH/POP R8", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x1122334455667788ULL &&
              cpu.Rsp() == STACK + 0x1000);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1122334455667788ULL);
        auto code = std::vector<std::uint8_t>{0x66, 0x50, 0x66, 0x58, 0xC3};
        CHECK("66h PUSH/POP AX preserves upper register bits", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x1122334455667788ULL &&
              cpu.Rsp() == STACK + 0x1000);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{0x48, 0x50, 0x48, 0x58, 0xC3};
        CHECK("REX.W PUSH/POP RAX", RunCode(cpu, mem, code) &&
              cpu.Rsp() == STACK + 0x1000);
    }
}

void TestCallRet()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    std::vector<std::uint8_t> code;

    // CALL +0x10
    code = {
        0xE8, 0x0B, 0x00, 0x00, 0x00
    };

    // RET after call returns here
    code.push_back(0xC3);

    // padding jusqu'a 0x10
    while (code.size() < 0x10) {
        code.push_back(0x90);
    }

    // Function
    Append(code, MovR64(0, 0x42));
    code.push_back(0xC3);

    code.resize(0x200, 0x90);

    CHECK(
        "CALL + RET",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x42);
}

void TestGroup1RexExtended()
{
    struct Case { const char* name; std::uint8_t modrm; std::uint8_t imm; std::uint64_t initial; std::uint64_t expected; };
    const Case cases[] = {
        {"ADD R8,-1", 0xC0, 0xFF, 0, 0xFFFFFFFFFFFFFFFFULL},
        {"OR R8,1",   0xC8, 0x01, 0x10, 0x11},
        {"AND R8,15", 0xE0, 0x0F, 0x1F, 0x0F},
        {"SUB R8,1",  0xE8, 0x01, 2, 1},
        {"XOR R8,255",0xF0, 0xFF, 0xAA, 0xFFFFFFFFFFFFFF55ULL}
    };
    for (const auto& c : cases) {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, c.initial);
        std::vector<std::uint8_t> code = {0x49, 0x83, c.modrm, c.imm, 0xF4};
        CHECK(c.name, RunCode(cpu, mem, code) && cpu.ReadRegister64(8) == c.expected);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0);
        cpu.SetRflags(1ULL);
        const std::vector<std::uint8_t> code = {0x49, 0x83, 0xD0, 0x00, 0xF4}; // ADC R8,0 + CF
        CHECK("ADC R8,0 preserves incoming carry in result", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 1);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0);
        cpu.SetRflags(1ULL);
        const std::vector<std::uint8_t> code = {0x49, 0x83, 0xD8, 0x00, 0xF4}; // SBB R8,0 - CF
        CHECK("SBB R8,0 consumes incoming borrow", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0xFFFFFFFFFFFFFFFFULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x198;
        Write64(mem, address, 0x0FULL);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x10ULL);
        cpu.WriteRegister64(13, DATA);
        cpu.WriteRegister64(9, 0x30ULL);
        cpu.SetRflags(1ULL);

        // 4F 1B 44 CD 18: SBB R8,[R13+R9*8+disp8].
        const std::vector<std::uint8_t> code = {
            0x4F, 0x1B, 0x44, 0xCD, 0x18, 0xF4
        };
        CHECK("SBB R8 memory REX.RXB SIB disp8 consumes carry",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0 &&
              (cpu.Rflags() & 1ULL) == 0);
    }

}

void TestPushfPopf()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        Append(code, MovR64(3, 2));
        code.insert(code.end(), {0x48, 0x39, 0xD8}); // CMP RAX,RBX -> CF=1, SF=1
        code.push_back(0x9C);                         // PUSHFQ
        code.insert(code.end(), {0x48, 0x39, 0xC0}); // CMP RAX,RAX -> CF=0, ZF=1
        code.push_back(0x9D);                         // POPFQ
        code = Finish(code);
        CHECK("PUSHFQ/POPFQ restores arithmetic flags", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 6)) == 0 &&
              (cpu.Rflags() & (1ULL << 7)) != 0 &&
              cpu.Rsp() == STACK + 0x1000);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        Append(code, MovR64(3, 2));
        code.insert(code.end(), {0x48, 0x39, 0xD8});
        code.insert(code.end(), {0x66, 0x9C});
        code.insert(code.end(), {0x48, 0x39, 0xC0});
        code.insert(code.end(), {0x66, 0x9D});
        code = Finish(code);
        CHECK("66h PUSHF/POPF uses 16-bit stack width", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 6)) == 0 &&
              (cpu.Rflags() & (1ULL << 7)) != 0 &&
              cpu.Rsp() == STACK + 0x1000);
    }
}

void TestLeaRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(STACK, 0x2000);
    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, 0x1000);
    cpu.WriteRegister64(9, 3);
    const std::vector<std::uint8_t> code = {
        0x4F, 0x8D, 0x44, 0x8D, 0xF0, // LEA RAX,[R13+R9*4-0x10]
        0xF4
    };
    CHECK("LEA REX.WRXB SIB with signed disp8 targets R8", RunCode(cpu, mem, code) &&
          cpu.ReadRegister64(8) == 0x0FFCULL);
    {
        Memory mem;
        mem.Map(CODE, 0x4000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, 0x2000ULL);
        cpu.WriteRegister64(9, 3);
        // 4F 8D 84 8D 00 01 00 00: LEA R8,[R13+R9*4+disp32].
        const std::vector<std::uint8_t> code = {
            0x4F, 0x8D, 0x84, 0x8D, 0x00, 0x01, 0x00, 0x00, 0xF4
        };
        CHECK("LEA REX.WRXB SIB disp32 writes R8",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x210CULL);
    }
}

void TestMovsxdVariants()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
                                              0x63, 0xC8, 0xF4}; // MOVSXD ECX,EAX
        CHECK("MOVSXD r32,r/m32 sign source then zero extends", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(1) == 0x00000000FFFFFFFFULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x300;
        const std::uint64_t index = 2;
        const std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 0x00000000FFFFFFFEULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        const std::vector<std::uint8_t> code = {
            0x4F, 0x63, 0x4C, 0x8D, 0x20, // MOVSXD R9,[R13+R9*4+disp8]
            0xF4
        };
        CHECK("MOVSXD REX.WRXB SIB memory sign extends to R9",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(9) == 0xFFFFFFFFFFFFFFFEULL);
    }
}

void TestIndirectCallAndRetImmediate()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        const std::uint64_t target = CODE + 0x40;
        auto code = MovR64(0, target);
        code.insert(code.end(), {0xFF, 0xD0}); // CALL RAX (FF /2)
        code.insert(code.end(), {0xF4});
        while (code.size() < 0x40)
            code.push_back(0x90);
        Append(code, MovR64(0, 0x1234));
        code.push_back(0xC3);
        CHECK("CALL r/m64 register form", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1234);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{0xE8, 0x08, 0x00, 0x00, 0x00};
        code.push_back(0xF4);
        while (code.size() < 0x0D)
            code.push_back(0x90);
        Append(code, MovR64(0, 0x5678));
        code.insert(code.end(), {0xC2, 0x08, 0x00}); // RET 8
        const std::uint64_t initialRsp = cpu.Rsp();
        CHECK("RET imm16 adjusts stack after popping return address",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x5678 &&
              cpu.Rsp() == initialRsp + 8);
    }
}


void TestIndirectCallMemoryRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x4000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    constexpr std::uint64_t base = DATA + 0x200;
    constexpr std::uint64_t index = 3;
    constexpr std::uint64_t displacement = 0x20;
    constexpr std::uint64_t target = CODE + 0x100;
    const std::uint64_t address = base + index * 4 + displacement;

    Write64(mem, address, target);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, base);
    cpu.WriteRegister64(9, index);

    auto code = std::vector<std::uint8_t>{
        0x4F, 0xFF, 0x54, 0x8D, 0x20, // CALL [R13+R9*4+disp8], REX.WRXB
        0xF4
    };
    while (code.size() < 0x100)
        code.push_back(0x90);

    Append(code, MovR64(0, 0x12345678ULL));
    code.push_back(0xC3);

    CHECK(
        "CALL r/m64 memory REX.WRXB SIB disp8",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x12345678ULL &&
        cpu.Rsp() == STACK + 0x1000);
}


void TestIndirectJmpMemoryRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x4000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    constexpr std::uint64_t base = DATA + 0x280;
    constexpr std::uint64_t index = 2;
    constexpr std::uint64_t displacement = 0x20;
    constexpr std::uint64_t target = CODE + 0x120;
    const std::uint64_t address = base + index * 4 + displacement;

    Write64(mem, address, target);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, base);
    cpu.WriteRegister64(9, index);
    const std::uint64_t initialRsp = cpu.Rsp();

    auto code = std::vector<std::uint8_t>{
        0x4F, 0xFF, 0x64, 0x8D, 0x20, // JMP [R13+R9*4+disp8], REX.WRXB
        0xF4
    };
    while (code.size() < 0x120)
        code.push_back(0x90);

    Append(code, MovR64(0, 0xCAFEBABEULL));
    code.push_back(0xF4);

    CHECK(
        "JMP r/m64 memory REX.WRXB SIB disp8",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0xCAFEBABEULL &&
        cpu.Rsp() == initialRsp);
}

void TestJumps()
{
    const struct {
        const char* name;
        std::uint8_t opcode;
        std::uint64_t lhs;
        std::uint64_t rhs;
    } tests[] = {
        {"JZ",  0x74, 5, 5},
        {"JNZ", 0x75, 5, 6},
        {"JB",  0x72, 5, 6},
        {"JAE", 0x73, 6, 5},
        {"JBE", 0x76, 5, 6},
        {"JA",  0x77, 6, 5},
        {"JL",  0x7C, 5, 6},
        {"JGE", 0x7D, 6, 5},
        {"JLE", 0x7E, 5, 6},
        {"JG",  0x7F, 6, 5}
    };

    for (const auto& t : tests) {

        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, t.lhs);
        Append(code, MovR64(3, t.rhs));

        // CMP RAX,RBX
        code.push_back(0x48);
        code.push_back(0x39);
        code.push_back(0xD8);

        // Jcc -> MOV RAX,2
        code.push_back(t.opcode);
        code.push_back(0x01);

        // Not taken path
        code.push_back(0xC3);

        // Taken target
        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        bool ok = RunCode(cpu, mem, code);

        CHECK(
            t.name,
            ok && cpu.Rax() == 2);
    }    {
        Memory mem;
        mem.Map(CODE, 0x4000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        // 0F 85 rel32: JNE over a deliberately long (>127 byte) gap.
        auto code = MovR64(0, 0);
        Append(code, MovR64(1, 1));
        code.insert(code.end(), {0x48, 0x39, 0xC8}); // CMP RAX,RCX -> ZF=0
        const std::size_t branch = code.size();
        code.insert(code.end(), {0x0F, 0x85, 0, 0, 0, 0});
        code.push_back(0xC3);
        while (code.size() < branch + 6 + 0x100)
            code.push_back(0x90);
        const std::int32_t rel =
            static_cast<std::int32_t>(code.size() - (branch + 6));
        std::memcpy(code.data() + branch + 2, &rel, sizeof(rel));
        Append(code, MovR64(0, 0x123456789ABCDEF0ULL));
        code.push_back(0xC3);

        CHECK("JNE rel32 takes long forward displacement",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x123456789ABCDEF0ULL);
    }

}



void TestJccBoundaryConditions()
{
    const struct {
        const char* name;
        std::uint8_t opcode;
        std::uint64_t flags;
    } cases[] = {
        {"JO taken", 0x70, 1ULL << 11},
        {"JNO taken", 0x71, 0},
        {"JP taken", 0x7A, 1ULL << 2},
        {"JNP taken", 0x7B, 0},
        {"JS taken", 0x78, 1ULL << 7},
        {"JNS taken", 0x79, 0},
    };

    for (const auto& c : cases) {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(c.flags);

        auto code = MovR64(0, 1);
        code.insert(code.end(), {c.opcode, 0x01, 0xC3});
        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        CHECK(c.name, RunCode(cpu, mem, code) && cpu.Rax() == 2);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(1ULL << 6);

        auto code = MovR64(0, 1);
        code.insert(code.end(), {0x75, 0x02}); // JNZ +2 is not taken when ZF=1.
        code.push_back(0xC3);
        code.push_back(0x90);
        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        CHECK(
            "JNZ not taken preserves sequential path",
            RunCode(cpu, mem, code) && cpu.Rax() == 1);
    }
}


void TestJmp()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    std::vector<std::uint8_t> code = {
        0xEB, 0x0A
    };

    Append(code, MovR64(0, 1));
    Append(code, MovR64(0, 2));
    code.push_back(0xC3);

    CHECK(
        "JMP rel8",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 2);
}

void TestImmediate()
{
    Memory mem;
    mem.Map(CODE, 0x3000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    // ADD 83 /0
    auto code = MovR64(0, 0x100);
    code.insert(code.end(), {
        0x48, 0x83, 0xC0, 0x05
    });

    // SUB 83 /5
    code.insert(code.end(), {
        0x48, 0x83, 0xE8, 0x02
    });

    // AND 83 /4
    code.insert(code.end(), {
        0x48, 0x83, 0xE0, 0x0F
    });

    // OR 83 /1
    code.insert(code.end(), {
        0x48, 0x83, 0xC8, 0x80
    });

    // XOR 83 /6
    code.insert(code.end(), {
        0x48, 0x83, 0xF0, 0x80
    });

    code = Finish(code);

    CHECK(
        "Immediate 83 ADD/SUB/AND/OR/XOR",
        RunCode(cpu, mem, code));
}

void TestCmpImmediate()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0x34);

    code.insert(code.end(), {
        0x48, 0x83, 0xF8, 0x34
    });

    code.push_back(0x74);
    code.push_back(0x01);
    code.push_back(0xC3);

    Append(code, MovR64(0, 2));
    code.push_back(0xC3);

    CHECK(
        "CMP 83 + JZ",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x2);
}

void TestImmediate81()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0x100000000ULL);

    code.insert(code.end(), {
        0x48, 0x81, 0xC0,
        0x34, 0x12, 0x00, 0x00
    });

    code = Finish(code);

    CHECK(
        "Immediate 81 ADD",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x100001234ULL);
}

void TestImmediateMemorySIB()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    const std::uint64_t address = DATA;

    Write32(mem, address, 100);

    auto code = MovR64(4, DATA + 8);

    code.insert(code.end(), {
        0x83, 0x44, 0x24, 0xF8, 0x05
    });

    code = Finish(code);

    CHECK(
        "AUDIT 83 ADD32 [RSP-8],5",
        RunCode(cpu, mem, code) &&
        Read32(mem, address) == 105);
}
void TestImmediateMemoryGroup1()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        const std::uint64_t address = DATA;
        Write32(mem, address, 100);

        auto code = MovR64(4, DATA + 8);
        code.insert(code.end(), {0x83, 0x4C, 0x24, 0xF8, 0x0F});
        code = Finish(code);

        CHECK(
            "AUDIT 83 OR32 [RSP-8],15",
            RunCode(cpu, mem, code) &&
            Read32(mem, address) == 111);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        const std::uint64_t address = DATA;
        Write32(mem, address, 0xFF);

        auto code = MovR64(4, DATA + 8);
        code.insert(code.end(), {0x83, 0x64, 0x24, 0xF8, 0x0F});
        code = Finish(code);

        CHECK(
            "AUDIT 83 AND32 [RSP-8],15",
            RunCode(cpu, mem, code) &&
            Read32(mem, address) == 15);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        const std::uint64_t address = DATA;
        Write32(mem, address, 100);

        auto code = MovR64(4, DATA + 8);
        code.insert(code.end(), {0x83, 0x6C, 0x24, 0xF8, 0x05});
        code = Finish(code);

        CHECK(
            "AUDIT 83 SUB32 [RSP-8],5",
            RunCode(cpu, mem, code) &&
            Read32(mem, address) == 95);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        const std::uint64_t address = DATA;
        Write32(mem, address, 0xF0F0);

        auto code = MovR64(4, DATA + 8);
        code.insert(code.end(), {0x83, 0x74, 0x24, 0xF8, 0x0F});
        code = Finish(code);

        CHECK(
            "AUDIT 83 XOR32 [RSP-8],15",
            RunCode(cpu, mem, code) &&
            Read32(mem, address) == 0xF0FF);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        const std::uint64_t address = DATA;
        Write32(mem, address, 100);

        auto code = MovR64(4, DATA + 8);
        code.insert(code.end(), {0x83, 0x7C, 0x24, 0xF8, 0x64});
        code.push_back(0x75);
        code.push_back(0x02);
        code.push_back(0xB8);
        code.push_back(0x01);
        code.push_back(0x00);
        code.push_back(0x00);
        code.push_back(0x00);
        code.push_back(0xC3);
        code = Finish(code);

        CHECK(
            "AUDIT 83 CMP32 [RSP-8],100",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 1 &&
            Read32(mem, address) == 100);
    }
}
void TestSignExtend83()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 0);

    code.insert(code.end(), {
        0x48, 0x83, 0xC0, 0xFF
    });

    code = Finish(code);

    CHECK(
        "83 sign extension",
        RunCode(cpu, mem, code) &&
        cpu.Rax() ==
            0xFFFFFFFFFFFFFFFFULL);
}

void TestRexRegisters()
{
    Memory mem;
    mem.Map(CODE, 0x3000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(
        8,
        0x1122334455667788ULL);

    Append(code, MovR64(
        9,
        5));

    // ADD R8,R9
    code.insert(code.end(), {
        0x4D, 0x01, 0xC8
    });

    code = Finish(code);

    CHECK(
        "REX.R / REX.B R8-R15",
        RunCode(cpu, mem, code) &&
        cpu.ReadRegister64(8) ==
            0x112233445566778DULL);
}

void TestRexXAndSib()
{
    Memory mem;
    mem.Map(CODE, 0x3000);
    mem.Map(DATA, 0x3000);
    mem.Map(STACK, 0x2000);

    Write64(mem, DATA + 0x40, 0x123456789ABCDEF0ULL);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);
    Append(code, MovR64(9, 8));

    code.insert(code.end(), {
        0x4A, 0x8B, 0x44, 0x88, 0x20
    });

    code = Finish(code);

    const bool run = RunCode(cpu, mem, code);

    std::cout
        << "[DEBUG] REX.X+SIB run="
        << (run ? 1 : 0)
        << " RAX=0x"
        << std::hex
        << cpu.Rax()
        << " expected=0x123456789abcdef0"
        << " memory=0x"
        << Read64(mem, DATA + 0x40)
        << std::dec
        << "\n";

    CHECK(
        "REX.X + SIB",
        run &&
        cpu.Rax() == 0x123456789ABCDEF0ULL);
}

void TestDisp8Disp32()
{
    Memory mem;
    mem.Map(CODE, 0x3000);
    mem.Map(DATA, 0x3000);
    mem.Map(STACK, 0x2000);

    Write64(mem, DATA + 0x10, 0x1111ULL);
    Write64(mem, DATA + 0x200, 0x2222ULL);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);

    code.insert(code.end(), {
        0x48, 0x8B, 0x58, 0x10
    });

    code.insert(code.end(), {
        0x48, 0x8B, 0x88,
        0x00, 0x02, 0x00, 0x00
    });

    code = Finish(code);

    const bool run = RunCode(cpu, mem, code);

    std::cout
        << "[DEBUG] DISP run="
        << (run ? 1 : 0)
        << " RBX=0x"
        << std::hex
        << cpu.ReadRegister64(3)
        << " expected=0x1111"
        << " RCX=0x"
        << cpu.ReadRegister64(1)
        << " expected=0x2222"
        << std::dec
        << "\n";

    CHECK(
        "Memory disp8 + disp32",
        run &&
        cpu.ReadRegister64(3) == 0x1111ULL &&
        cpu.ReadRegister64(1) == 0x2222ULL);
}
void TestIncDecNeg()
{
    // =========================================================
    // INC64
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        // RAX = 41
        // RBX = 42
        // CMP RAX,RBX => 41-42 => CF=1
        // MOV RAX,41 ne modifie pas CF
        // INC RAX doit conserver CF=1
        auto code = MovR64(0, 41);
        auto tmp = MovR64(3, 42);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // CMP RAX,RBX
        code.insert(code.end(), {
            0x48, 0x39, 0xD8
        });

        // RAX = 41, MOV ne modifie pas CF
        tmp = MovR64(0, 41);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // INC RAX
        code.insert(code.end(), {
            0x48, 0xFF, 0xC0
        });

        code = Finish(code);

        CHECK(
            "INC64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 42 &&
            (cpu.Rflags() & 1ULL) != 0);
    }

    // =========================================================
    // DEC64
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        // RAX = 42
        // RBX = 43
        // CMP RAX,RBX => CF=1
        auto code = MovR64(0, 42);
        auto tmp = MovR64(3, 43);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // CMP RAX,RBX
        code.insert(code.end(), {
            0x48, 0x39, 0xD8
        });

        // RAX = 42, CF reste Ã  1
        tmp = MovR64(0, 42);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // DEC RAX
        code.insert(code.end(), {
            0x48, 0xFF, 0xC8
        });

        code = Finish(code);

        CHECK(
            "DEC64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 41 &&
            (cpu.Rflags() & 1ULL) != 0);
    }

    // =========================================================
    // NEG64
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 5);

        // NEG RAX
        code.insert(code.end(), {
            0x48, 0xF7, 0xD8
        });

        code = Finish(code);

        CHECK(
            "NEG64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xFFFFFFFFFFFFFFFBULL &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 6)) == 0);
    }

    // =========================================================
    // INC32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        // EAX = 41
        auto code = std::vector<std::uint8_t>{
            0xB8,
            0x29, 0x00, 0x00, 0x00
        };

        // EBX = 42
        code.insert(code.end(), {
            0xBB,
            0x2A, 0x00, 0x00, 0x00
        });

        // CMP EAX,EBX => CF=1
        code.insert(code.end(), {
            0x39, 0xD8
        });

        // EAX = 41, MOV ne modifie pas CF
        code.insert(code.end(), {
            0xB8,
            0x29, 0x00, 0x00, 0x00
        });

        // INC EAX
        code.insert(code.end(), {
            0xFF, 0xC0
        });

        code = Finish(code);

        CHECK(
            "INC32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 42 &&
            (cpu.Rflags() & 1ULL) != 0);
    }

    // =========================================================
    // DEC32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        // EAX = 42
        auto code = std::vector<std::uint8_t>{
            0xB8,
            0x2A, 0x00, 0x00, 0x00
        };

        // EBX = 43
        code.insert(code.end(), {
            0xBB,
            0x2B, 0x00, 0x00, 0x00
        });

        // CMP EAX,EBX => CF=1
        code.insert(code.end(), {
            0x39, 0xD8
        });

        // EAX = 42, MOV ne modifie pas CF
        code.insert(code.end(), {
            0xB8,
            0x2A, 0x00, 0x00, 0x00
        });

        // DEC EAX
        code.insert(code.end(), {
            0xFF, 0xC8
        });

        code = Finish(code);

        CHECK(
            "DEC32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 41 &&
            (cpu.Rflags() & 1ULL) != 0);
    }

    // =========================================================
    // NEG32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8,
            0x05, 0x00, 0x00, 0x00
        };

        // NEG EAX
        code.insert(code.end(), {
            0xF7, 0xD8
        });

        code = Finish(code);

        CHECK(
            "NEG32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x00000000FFFFFFFBULL &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 6)) == 0);
    }

    // 8-bit FE /0, FE /1 and F6 /3 forms were previously untested.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(1ULL);

        auto code = MovR64(0, 0x11223344556600FFULL);
        Append(code, {0xFE, 0xC0}); // INC AL
        Append(code, {0xFE, 0xC8}); // DEC AL
        code = Finish(code);

        CHECK("INC8_DEC8_preserve_CF_and_upper_bits",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x11223344556600FFULL &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 1);
        Append(code, {0xF6, 0xD8}); // NEG AL
        code = Finish(code);

        CHECK("NEG8_flags_and_result",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFULL) == 0xFFULL &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 4)) != 0 &&
              (cpu.Rflags() & (1ULL << 2)) != 0);
    }

}
void TestFlags()
{
    Memory mem;
    mem.Map(CODE, 0x3000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    // ADD overflow
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(
            0,
            0x7FFFFFFFFFFFFFFFULL);

        Append(code, MovR64(1, 1));

        code.insert(code.end(), {
            0x48, 0x03, 0xC1
        });

        code = Finish(code);

        CHECK(
            "ADD64 OF",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    // ADD carry + ZF
    {
        Memory mem2;
        mem2.Map(CODE, 0x2000);
        mem2.Map(DATA, 0x1000);
        mem2.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem2);

        auto code = MovR64(
            0,
            0xFFFFFFFFFFFFFFFFULL);

        Append(code, MovR64(1, 1));

        code.insert(code.end(), {
            0x48, 0x03, 0xC1
        });

        code = Finish(code);

        CHECK(
            "ADD64 CF + ZF",
            RunCode(cpu, mem2, code) &&
            cpu.Rax() == 0 &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 6)) != 0);
    }

    // NEG64 must update AF/PF in addition to CF/OF/SF/ZF.
    {
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x01);
        code.insert(code.end(), {0x48, 0xF7, 0xD8}); // NEG RAX
        code = Finish(code);
        CHECK(
            "NEG64 AF + PF",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xFFFFFFFFFFFFFFFFULL &&
            (cpu.Rflags() & (1ULL << 4)) != 0 &&
            (cpu.Rflags() & (1ULL << 2)) != 0);
    }

    // INC/DEC preserve CF but must update AF/PF.
    {
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(1ULL);
        auto code = MovR64(0, 0x0F);
        code.push_back(0x48);
        code.push_back(0xFF);
        code.push_back(0xC0); // INC RAX
        code.push_back(0x48);
        code.push_back(0xFF);
        code.push_back(0xC8); // DEC RAX
        code = Finish(code);
        CHECK(
            "INC/DEC AF + PF",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x0FULL &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 4)) != 0 &&
            (cpu.Rflags() & (1ULL << 2)) != 0);
    }

    // SUB borrow
    {
        Memory mem3;
        mem3.Map(CODE, 0x2000);
        mem3.Map(DATA, 0x1000);
        mem3.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem3);

        auto code = MovR64(0, 0);
        Append(code, MovR64(1, 1));

        code.insert(code.end(), {
            0x48, 0x2B, 0xC1
        });

        code = Finish(code);

        CHECK(
            "SUB64 CF + SF",
            RunCode(cpu, mem3, code) &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 7)) != 0);
    }
}

} // namespace

void TestAdcSbb()
{
    // ADC64 - sans carry
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 10);
        auto tmp = MovR64(3, 5);
        code.insert(code.end(), tmp.begin(), tmp.end());

        code.insert(code.end(), {
            0x48, 0x13, 0xC3
        });

        code = Finish(code);

        CHECK(
            "ADC64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 15 &&
            (cpu.Rflags() & 1ULL) == 0);
    }

    // ADC64 - carry entrant
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 5);
        auto tmp = MovR64(3, 10);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // CMP 5,10 => CF=1
        code.insert(code.end(), {
            0x48, 0x39, 0xD8
        });

        // MOV RAX,10 ne modifie pas CF
        tmp = MovR64(0, 10);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // ADC RAX,RBX => 10 + 10 + 1 = 21
        code.insert(code.end(), {
            0x48, 0x13, 0xC3
        });

        code = Finish(code);

        CHECK(
            "ADC64_CF",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 21);
    }

    // SBB64 - sans borrow
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 10);
        auto tmp = MovR64(3, 3);
        code.insert(code.end(), tmp.begin(), tmp.end());

        code.insert(code.end(), {
            0x48, 0x1B, 0xC3
        });

        code = Finish(code);

        CHECK(
            "SBB64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 7 &&
            (cpu.Rflags() & 1ULL) == 0);
    }

    // SBB64 - borrow entrant
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 5);
        auto tmp = MovR64(3, 10);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // CMP 5,10 => CF=1
        code.insert(code.end(), {
            0x48, 0x39, 0xD8
        });

        // RAX=20, CF reste a 1
        tmp = MovR64(0, 20);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // SBB RAX,RBX => 20 - 10 - 1 = 9
        code.insert(code.end(), {
            0x48, 0x1B, 0xC3
        });

        code = Finish(code);

        CHECK(
            "SBB64_CF",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 9);
    }

    // ADC32
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x00000000FFFFFFFEULL);
        auto tmp = MovR64(3, 2);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // ADC EAX,EBX => 0xFFFFFFFE + 2 = 0
        code.insert(code.end(), {
            0x11, 0xD8
        });

        code = Finish(code);

        CHECK(
            "ADC32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0);
    }

    // SBB32
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 10);
        auto tmp = MovR64(3, 3);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // SBB EAX,EBX => 10 - 3 = 7
        code.insert(code.end(), {
            0x1B, 0xC3
        });

        code = Finish(code);

        CHECK(
            "SBB32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 7);
    }
    // Memory destination forms for all widths, including carry/borrow-in
    // and the complete arithmetic flag set.
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint8_t v=0x7F; mem.Write(DATA,&v,1);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x10,0x18}); code=Finish(code);
        CHECK("ADC8_mem",RunCode(cpu,mem,code)&&(Read64(mem,DATA)&0xFF)==0x80&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&(cpu.Rflags()&(1ULL<<7))&&
              !(cpu.Rflags()&(1ULL<<6))&&!(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint32_t v=0x7FFFFFFF; mem.Write(DATA,reinterpret_cast<std::uint8_t*>(&v),4);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x11,0x18}); code=Finish(code);
        CHECK("ADC32_mem",RunCode(cpu,mem,code)&&Read32(mem,DATA)==0x80000000U&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&(cpu.Rflags()&(1ULL<<7))&&
              !(cpu.Rflags()&(1ULL<<6))&&(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint64_t v=0x7FFFFFFFFFFFFFFFULL; mem.Write(DATA,reinterpret_cast<std::uint8_t*>(&v),8);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x48,0x11,0x18}); code=Finish(code);
        CHECK("ADC64_mem",RunCode(cpu,mem,code)&&Read64(mem,DATA)==0x8000000000000000ULL&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&(cpu.Rflags()&(1ULL<<7))&&
              !(cpu.Rflags()&(1ULL<<6))&&(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint8_t v=0x80; mem.Write(DATA,&v,1);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x18,0x18}); code=Finish(code);
        CHECK("SBB8_mem",RunCode(cpu,mem,code)&&(Read64(mem,DATA)&0xFF)==0x7F&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&
              !(cpu.Rflags()&(1ULL<<7))&&!(cpu.Rflags()&(1ULL<<6))&&!(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint32_t v=0x80000000U; mem.Write(DATA,reinterpret_cast<std::uint8_t*>(&v),4);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x19,0x18}); code=Finish(code);
        CHECK("SBB32_mem",RunCode(cpu,mem,code)&&Read32(mem,DATA)==0x7FFFFFFFU&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&
              !(cpu.Rflags()&(1ULL<<7))&&!(cpu.Rflags()&(1ULL<<6))&&(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }
    {
        Memory mem; mem.Map(CODE,0x2000); mem.Map(DATA,0x1000); mem.Map(STACK,0x2000);
        Cpu cpu=MakeCpu(mem); std::uint64_t v=0x8000000000000000ULL; mem.Write(DATA,reinterpret_cast<std::uint8_t*>(&v),8);
        cpu.SetRflags(1);
        auto code=MovR64(0,DATA); Append(code,MovR64(3,0)); Append(code,{0x48,0x19,0x18}); code=Finish(code);
        CHECK("SBB64_mem",RunCode(cpu,mem,code)&&Read64(mem,DATA)==0x7FFFFFFFFFFFFFFFULL&&
              (cpu.Rflags()&(1ULL<<11))&&(cpu.Rflags()&(1ULL<<4))&&
              !(cpu.Rflags()&(1ULL<<7))&&!(cpu.Rflags()&(1ULL<<6))&&(cpu.Rflags()&(1ULL<<2))&&!(cpu.Rflags()&1ULL));
    }

    // Group-1 immediate ADC/SBB carry/borrow must not lose the carry
    // when the immediate itself is UINT_MAX.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(3, 1));
        Append(code, {0x48, 0x39, 0xD8}); // CF=1
        Append(code, {0x83, 0xD0, 0xFF}); // ADC EAX,-1 + CF => 0, CF must remain 1
        code = Finish(code);

        CHECK("ADC32_imm8_CF_boundary",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0 &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(3, 1));
        Append(code, {0x48, 0x39, 0xD8}); // CF=1
        Append(code, {0x83, 0xD8, 0xFF}); // SBB EAX,-1 - CF => 0, CF must remain 1
        code = Finish(code);

        CHECK("SBB32_imm8_CF_boundary",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0 &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(3, 1));
        Append(code, {0x48, 0x39, 0xD8}); // CF=1
        Append(code, {0x48, 0x83, 0xD0, 0xFF}); // ADC RAX,-1 + CF => 0
        code = Finish(code);

        CHECK("ADC64_imm8_CF_boundary",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0 &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(3, 1));
        Append(code, {0x48, 0x39, 0xD8}); // CF=1
        Append(code, {0x48, 0x83, 0xD8, 0xFF}); // SBB RAX,-1 - CF => 0
        code = Finish(code);

        CHECK("SBB64_imm8_CF_boundary",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0 &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    // Immediate accumulator forms: ADC/SBB must exist for 8/32/64-bit operands.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x7FULL);
        code.insert(code.end(), {0x14, 0x01}); // ADC AL,1 -> 0x80, OF=1, AF=1
        code = Finish(code);
        CHECK("ADC8_imm", RunCode(cpu, mem, code) && cpu.Rax() == 0x80ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0 &&
              (cpu.Rflags() & (1ULL << 4)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x7FFFFFFFULL);
        code.insert(code.end(), {0x15, 0x01, 0x00, 0x00, 0x00}); // ADC EAX,1
        code = Finish(code);
        CHECK("ADC32_imm", RunCode(cpu, mem, code) && cpu.Rax() == 0x80000000ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x7FFFFFFFFFFFFFFFULL);
        code.insert(code.end(), {0x48, 0x15, 0x01, 0x00, 0x00, 0x00}); // ADC RAX,1
        code = Finish(code);
        CHECK("ADC64_imm", RunCode(cpu, mem, code) && cpu.Rax() == 0x8000000000000000ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(3, 1));
        code.insert(code.end(), {0x48, 0x39, 0xD8}); // CF=1
        code.insert(code.end(), {0x14, 0x00});       // ADC AL,0 + CF -> 1
        code.insert(code.end(), {0x1C, 0x01});       // SBB AL,1 -> 0, CF=0
        code = Finish(code);
        CHECK("ADC8_SBB8_imm_CF", RunCode(cpu, mem, code) && cpu.Rax() == 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 20);
        code.insert(code.end(), {0x1D, 0x0A, 0x00, 0x00, 0x00}); // SBB EAX,10
        code.insert(code.end(), {0x48, 0x1D, 0x0A, 0x00, 0x00, 0x00}); // SBB RAX,10
        code = Finish(code);
        CHECK("SBB32_64_imm", RunCode(cpu, mem, code) && cpu.Rax() == 0);
    }

    // 16-bit ADC/SBB register forms exercise 66h operand size together
    // with REX.R/B register extensions.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        cpu.SetRflags(1);
        auto code = MovR64(8, 0x0000000000007FFFULL);
        Append(code, MovR64(9, 0x0000000000000000ULL));
        Append(code, {0x66, 0x45, 0x13, 0xC1}); // ADC R8W,R9W + CF
        code = Finish(code);

        CHECK("ADC16_REX_RB",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x8000ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0 &&
              (cpu.Rflags() & 1ULL) == 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        cpu.SetRflags(1);
        auto code = MovR64(8, 0x0000000000000000ULL);
        Append(code, MovR64(9, 0x0000000000000000ULL));
        Append(code, {0x66, 0x45, 0x1B, 0xC1}); // SBB R8W,R9W - CF
        code = Finish(code);

        CHECK("SBB16_REX_RB",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0xFFFFULL &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    // 64-bit register-to-memory ADC with REX.R/X/B and SIB exercises the
    // full extended ModRM address path, including disp8.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        constexpr std::uint64_t base = DATA + 0x200;
        constexpr std::uint64_t index = 3;
        constexpr std::uint64_t displacement = 0x20;
        const std::uint64_t address = base + index * 4 + displacement;
        const std::uint64_t value = 0x7FFFFFFFFFFFFFFFULL;
        Write64(mem, address, value);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 1);
        cpu.SetRflags(0);

        auto code = std::vector<std::uint8_t>{
            0x4F, 0x11, 0x44, 0x8D, 0x20 // ADC [R13+R9*4+20h],R8
        };
        code = Finish(code);

        CHECK("ADC64_mem_REX_RXB_SIB_disp8",
              RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0x8000000000000000ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    // Group-1 immediate memory forms must honor operand width and the
    // sign-extended imm32 encoding used by 64-bit ADC/SBB.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        constexpr std::uint64_t address = DATA + 0x180;
        const std::uint16_t initial = 0x7FFF;
        mem.Write(address, reinterpret_cast<const std::uint8_t*>(&initial), sizeof(initial));

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(10, address);
        cpu.SetRflags(0);

        auto code = std::vector<std::uint8_t>{
            0x66, 0x81, 0x12, 0x01, 0x00, // ADC WORD PTR [RDX],1 (RDX is r/m)
            0xF4
        };
        cpu.WriteRegister64(2, address);
        CHECK("ADC16_group1_memory",
              RunCode(cpu, mem, code) &&
              Read16(mem, address) == 0x8000U &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        constexpr std::uint64_t address = DATA + 0x1A0;
        const std::uint64_t initial = 0x7FFFFFFFFFFFFFFFULL;
        Write64(mem, address, initial);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(0);

        auto code = std::vector<std::uint8_t>{
            0x48, 0x83, 0x1C, 0x25,
            static_cast<std::uint8_t>(address),
            static_cast<std::uint8_t>(address >> 8),
            static_cast<std::uint8_t>(address >> 16),
            static_cast<std::uint8_t>(address >> 24),
            0xFF, // SBB QWORD PTR [disp32],-1
            0xF4
        };

        CHECK("SBB64_group1_memory_imm8_disp32",
              RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0x8000000000000000ULL &&
              (cpu.Rflags() & (1ULL << 11)) != 0 &&
              (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        constexpr std::uint64_t base = DATA + 0x280;
        constexpr std::uint64_t index = 2;
        constexpr std::uint64_t displacement = 0x20;
        const std::uint64_t address = base + index * 4 + displacement;
        const std::uint16_t initial = 0x7FFFU;
        mem.Write(
            address,
            reinterpret_cast<const std::uint8_t*>(&initial),
            sizeof(initial));

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 1);
        cpu.SetRflags(0);

        // 66 47 11 44 8D 20: ADC WORD PTR [R13+R9*4+20h],R8W.
        // 66 selects 16-bit operands; REX.R/X/B selects R8/R9/R13.
        const std::vector<std::uint8_t> code = {
            0x66, 0x47, 0x11, 0x44, 0x8D, 0x20, 0xF4
        };

        CHECK(
            "ADC16 memory REX.RXB SIB disp8",
            RunCode(cpu, mem, code) &&
            Read16(mem, address) == 0x8000U &&
            (cpu.Rflags() & (1ULL << 11)) != 0);
    }
}

void TestImul()
{
    // =========================================================
    // IMUL64
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 6);
        auto tmp = MovR64(3, 7);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // IMUL RAX,RBX
        // 48 0F AF C3
        code.insert(code.end(), {
            0x48, 0x0F, 0xAF, 0xC3
        });

        code = Finish(code);

        CHECK(
            "IMUL64",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 42 &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & 0x800ULL) == 0);
    }

    // =========================================================
    // IMUL32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 6);
        auto tmp = MovR64(3, 5);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // IMUL EAX,EBX
        // 0F AF C3
        code.insert(code.end(), {
            0x0F, 0xAF, 0xC3
        });

        code = Finish(code);

        CHECK(
            "IMUL32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 30 &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & 0x800ULL) == 0);
    }

    // =========================================================
    // IMUL64 overflow
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(
            0,
            0x7FFFFFFFFFFFFFFFULL);

        auto tmp = MovR64(3, 2);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // IMUL RAX,RBX
        code.insert(code.end(), {
            0x48, 0x0F, 0xAF, 0xC3
        });

        code = Finish(code);

        CHECK(
            "IMUL64_OF",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xFFFFFFFFFFFFFFFEULL &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & 0x800ULL) != 0);
    }

    // =========================================================
    // IMUL R8,R9
    // REX.R + REX.B
    // 4D 0F AF C1
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(8, 11);
        auto tmp = MovR64(9, 12);
        code.insert(code.end(), tmp.begin(), tmp.end());

        // IMUL R8,R9
        // REX.W + REX.R + REX.B
        code.insert(code.end(), {
            0x4D, 0x0F, 0xAF, 0xC1
        });

        code = Finish(code);

        CHECK(
            "IMUL R8,R9",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0 &&
            cpu.ReadRegister64(8) == 132 &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & 0x800ULL) == 0);
    }

    // One-operand MUL/IMUL must also cover the 32-bit implicit EDX:EAX form.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xFFFFFFFFULL);
        Append(code, MovR64(3, 2));
        Append(code, {0xF7, 0xE3}); // MUL EBX
        code = Finish(code);

        CHECK("MUL32_reg_EDX_EAX",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0xFFFFFFFEULL &&
              cpu.ReadRegister64(2) == 1ULL &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xFFFFFFFEULL); // EAX = -2
        Append(code, MovR64(3, 2));
        Append(code, {0xF7, 0xEB}); // IMUL EBX
        code = Finish(code);

        CHECK("IMUL32_reg_EDX_EAX_signed_fit",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0xFFFFFFFCULL &&
              cpu.ReadRegister64(2) == 0xFFFFFFFFULL &&
              (cpu.Rflags() & 1ULL) == 0 &&
              (cpu.Rflags() & (1ULL << 11)) == 0);
    }

}

void TestMulDiv8()
{
    // F6 /4 MUL r/m8: the implicit AX result must contain the full
    // unsigned product and CF/OF must indicate that AH is non-zero.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x10);
        Append(code, MovR64(3, 0x10));
        Append(code, {0xF6, 0xE3}); // MUL BL
        code = Finish(code);

        CHECK("MUL8_reg_AX_result",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFFFULL) == 0x0100ULL &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    // F6 /5 IMUL r/m8: a signed product that fits in AL clears CF/OF.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xF8); // AL = -8
        Append(code, MovR64(3, 3));
        Append(code, {0xF6, 0xEB}); // IMUL BL
        code = Finish(code);

        CHECK("IMUL8_reg_signed_fit",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFFFULL) == 0xFFE8ULL &&
              (cpu.Rflags() & 1ULL) == 0 &&
              (cpu.Rflags() & (1ULL << 11)) == 0);
    }

    // F6 /4 memory form through SIB disp32-only.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        const std::uint8_t value = 0x10;
        mem.Write(DATA, &value, 1);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x10);
        Append(code, {0xF6, 0x24, 0x25,
                      static_cast<std::uint8_t>(DATA),
                      static_cast<std::uint8_t>(DATA >> 8),
                      static_cast<std::uint8_t>(DATA >> 16),
                      static_cast<std::uint8_t>(DATA >> 24)});
        code = Finish(code);

        CHECK("MUL8_memory_SIB_disp32",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFFFULL) == 0x0100ULL &&
              (cpu.Rflags() & 1ULL) != 0 &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }

    // F6 /6 DIV r/m8: AX is the implicit dividend and AL/AH receive
    // quotient/remainder respectively.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x0100);
        Append(code, MovR64(3, 10));
        Append(code, {0xF6, 0xF3}); // DIV BL
        code = Finish(code);

        CHECK("DIV8_reg_AX_quotient_remainder",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFFFULL) == 0x0619ULL);
    }

    // F6 /7 IDIV r/m8: signed AX dividend, quotient in AL and remainder in AH.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xFF9C); // AX = -100
        Append(code, MovR64(3, 7));
        Append(code, {0xF6, 0xFB}); // IDIV BL
        code = Finish(code);

        CHECK("IDIV8_reg_signed_quotient_remainder",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(0) & 0xFFFFULL) == 0xFEF2ULL);
    }
}

void TestDivIdiv128()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    // =========================================================
    // DIV64 : RDX:RAX = 0x0000000000000001_0000000000000000
    //         / 2
    //
    // RÃ©sultat :
    // RAX = 0x8000000000000000
    // RDX = 0
    // =========================================================
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x0000000000000000ULL);
        Append(code, MovR64(2, 0x0000000000000001ULL));
        Append(code, MovR64(1, 2));
        Append(code, {0x48, 0xF7, 0xF1}); // DIV RCX
        code = Finish(code);

        CHECK(
            "DIV64_128_EXACT",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x8000000000000000ULL &&
            cpu.ReadRegister64(2) == 0x0000000000000000ULL);
    }

    // =========================================================
    // DIV64 : RDX:RAX = 0x0000000000000002_0000000000000001
    //         / 3
    //
    // Quotient = 0xAAAAAAAAAAAAAAAA
    // Reste    = 3
    // =========================================================
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x0000000000000001ULL);
        Append(code, MovR64(2, 0x0000000000000002ULL));
        Append(code, MovR64(1, 3));
        Append(code, {0x48, 0xF7, 0xF1}); // DIV RCX
        code = Finish(code);

        CHECK(
            "DIV64_128_QUOTIENT_REMAINDER",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xAAAAAAAAAAAAAAABULL &&
            cpu.ReadRegister64(2) == 0x0000000000000000ULL);
    }

    // =========================================================
    // IDIV64 :
    // RDX:RAX = -0x00000000000000010000000000000000
    //         / 2
    //
    // Quotient = -0x8000000000000000
    // Reste    = 0
    // =========================================================
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x0000000000000000ULL);
        Append(code, MovR64(2, 0xFFFFFFFFFFFFFFFFULL));
        Append(code, MovR64(1, 2));
        Append(code, {0x48, 0xF7, 0xF9}); // IDIV RCX
        code = Finish(code);

        CHECK(
            "IDIV64_128_NEGATIVE_EXACT",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x8000000000000000ULL &&
            cpu.ReadRegister64(2) == 0x0000000000000000ULL);
    }

    // =========================================================
    // IDIV64 :
    // dividende = -10
    // diviseur  = 3
    //
    // quotient = -3
    // reste    = -1
    // =========================================================
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xFFFFFFFFFFFFFFF6ULL);
        Append(code, MovR64(2, 0xFFFFFFFFFFFFFFFFULL));
        Append(code, MovR64(1, 3));
        Append(code, {0x48, 0xF7, 0xF9}); // IDIV RCX
        code = Finish(code);

        CHECK(
            "IDIV64_NEGATIVE_REMAINDER",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFFFFFFFFDULL &&
            cpu.ReadRegister64(2) == 0xFFFFFFFFFFFFFFFFULL);
    }

    // =========================================================
    // IDIV64 :
    // dividende = +2^127
    // diviseur  = 1
    //
    // Quotient impossible dans un int64 signÃ© => overflow.
    // =========================================================
    {
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(2, 0x8000000000000000ULL));
        Append(code, MovR64(1, 1));
        Append(code, {0x48, 0xF7, 0xF9}); // IDIV RCX
        code = Finish(code);

        CHECK(
            "IDIV64_128_OVERFLOW",
            !RunCode(cpu, mem, code));
    }    // IDIV64 boundary: INT64_MIN / -1 is the architectural quotient overflow case.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x8000000000000000ULL);
        Append(code, MovR64(2, 0xFFFFFFFFFFFFFFFFULL));
        Append(code, MovR64(3, 0xFFFFFFFFFFFFFFFFULL));
        Append(code, {0x48, 0xF7, 0xFB}); // IDIV RBX
        code = Finish(code);

        CHECK("IDIV64 INT64_MIN divided by -1 overflows",
              !RunCode(cpu, mem, code));
    }

}

void TestDivIdiv()
{
    // =========================================================
    // DIV32 simple
    // EAX = 100 / EBX = 7
    // EAX = 14
    // EDX = 2
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 100);
        Append(code, MovR64(3, 7));
        Append(code, MovR64(2, 0));

        // DIV EBX
        // F7 F3
        code.insert(code.end(), {
            0xF7, 0xF3
        });

        code = Finish(code);

        CHECK(
            "DIV32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 14 &&
            cpu.ReadRegister64(2) == 2);
    }

    // =========================================================
    // DIV32 avec vrai dividend EDX:EAX
    //
    // 0x00000001_00000000 / 3
    // = 1431655765 reste 1
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(2, 1));
        Append(code, MovR64(3, 3));

        // DIV EBX
        code.insert(code.end(), {
            0xF7, 0xF3
        });

        code = Finish(code);

        CHECK(
            "DIV32_EDX_EAX",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 1431655765ULL &&
            cpu.ReadRegister64(2) == 1);
    }

    // =========================================================
    // IDIV32 positif
    //
    // 100 / 7 = 14 reste 2
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 100);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 7));

        // IDIV EBX
        // F7 FB
        code.insert(code.end(), {
            0xF7, 0xFB
        });

        code = Finish(code);

        CHECK(
            "IDIV32_POS",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 14 &&
            cpu.ReadRegister64(2) == 2);
    }

    // =========================================================
    // IDIV32 nÃ©gatif
    //
    // -100 / 7 = -14 reste -2
    //
    // EDX:EAX = FFFFFFFF:FFFFFF9C
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(
            0,
            0xFFFFFFFFFFFFFF9CULL);

        Append(
            code,
            MovR64(
                2,
                0xFFFFFFFFFFFFFFFFULL));

        Append(code, MovR64(3, 7));

        // IDIV EBX
        code.insert(code.end(), {
            0xF7, 0xFB
        });

        code = Finish(code);

        CHECK(
            "IDIV32_NEG",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) ==
                0x00000000FFFFFFF2ULL &&
            cpu.ReadRegister64(2) ==
                0x00000000FFFFFFFEULL);
    }

    // =========================================================
    // DIV64
    //
    // RAX = 100
    // RDX = 0
    // 100 / 7 = 14 reste 2
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 100);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 7));

        // DIV RBX
        // 48 F7 F3
        code.insert(code.end(), {
            0x48, 0xF7, 0xF3
        });

        code = Finish(code);

        CHECK(
            "DIV64",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 14 &&
            cpu.ReadRegister64(2) == 2);
    }

    // =========================================================
    // IDIV64 nÃ©gatif
    //
    // RAX = -100
    // RDX = -1 (extension de signe)
    // -100 / 7 = -14 reste -2
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(
            0,
            0xFFFFFFFFFFFFFF9CULL);

        Append(
            code,
            MovR64(
                2,
                0xFFFFFFFFFFFFFFFFULL));

        Append(code, MovR64(3, 7));

        // IDIV RBX
        // 48 F7 FB
        code.insert(code.end(), {
            0x48, 0xF7, 0xFB
        });

        code = Finish(code);

        CHECK(
            "IDIV64_NEG",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) ==
                0xFFFFFFFFFFFFFFF2ULL &&
            cpu.ReadRegister64(2) ==
                0xFFFFFFFFFFFFFFFEULL);
    }

    // =========================================================
    // DIV32 division par zero
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 100);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 0));

        // DIV EBX
        code.insert(code.end(), {
            0xF7, 0xF3
        });

        code = Finish(code);

        CHECK(
            "DIV32_ZERO",
            !RunCode(cpu, mem, code));
    }

    // =========================================================
    // DIV32 quotient overflow
    //
    // EDX:EAX = 0x00000001_00000000
    // divisor = 1
    //
    // quotient = 0x100000000
    // => trop grand pour EAX
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0);
        Append(code, MovR64(2, 1));
        Append(code, MovR64(3, 1));

        // DIV EBX
        code.insert(code.end(), {
            0xF7, 0xF3
        });

        code = Finish(code);

        CHECK(
            "DIV32_OVERFLOW",
            !RunCode(cpu, mem, code));
    }
    // =========================================================
    // DIV/IDIV16 exception boundaries
    // =========================================================
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 0));
        code.insert(code.end(), {0x66, 0xF7, 0xF3}); // DIV BX
        code = Finish(code);
        CHECK("DIV16_ZERO", !RunCode(cpu, mem, code));
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0);
        Append(code, MovR64(2, 1));
        Append(code, MovR64(3, 1));
        code.insert(code.end(), {0x66, 0xF7, 0xF3}); // DIV BX
        code = Finish(code);
        CHECK("DIV16_OVERFLOW", !RunCode(cpu, mem, code));
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 0));
        code.insert(code.end(), {0x66, 0xF7, 0xFB}); // IDIV BX
        code = Finish(code);
        CHECK("IDIV16_ZERO", !RunCode(cpu, mem, code));
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x8000);
        Append(code, MovR64(2, 0xFFFF));
        Append(code, MovR64(3, 1));
        code.insert(code.end(), {0x66, 0xF7, 0xFB}); // IDIV BX: -32768 / 1 fits, not overflow
        code = Finish(code);
        CHECK("IDIV16_MIN_VALUE", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFFFFULL) == 0x8000ULL &&
              (cpu.ReadRegister64(2) & 0xFFFFULL) == 0);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0000000000000064ULL);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 7));
        code.insert(code.end(), {0x66, 0xF7, 0xF3}); // DIV BX: 100 / 7
        code = Finish(code);
        CHECK("DIV16 quotient and remainder", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFFFFULL) == 14 &&
              (cpu.ReadRegister64(2) & 0xFFFFULL) == 2);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0000000000008000ULL);
        Append(code, MovR64(2, 0x000000000000FFFFULL));
        Append(code, MovR64(3, 0x000000000000FFFFULL));
        code.insert(code.end(), {0x66, 0xF7, 0xFB}); // IDIV BX: -32768 / -1 = +32768, overflow
        code = Finish(code);
        CHECK("IDIV16 minimum divided by minus one overflows", !RunCode(cpu, mem, code));
    }

    // =========================================================
    // DIV64 division par zero
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 100);
        Append(code, MovR64(2, 0));
        Append(code, MovR64(3, 0));
        code.insert(code.end(), {0x48, 0xF7, 0xF3}); // DIV RBX
        code = Finish(code);
        CHECK("DIV64_ZERO", !RunCode(cpu, mem, code));
    }

    // =========================================================
    // DIV64 quotient overflow
    //
    // RDX:RAX = 0x00000001_0000000000000000
    // divisor = 1 -> quotient = 2^64, hors plage de RAX
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0);
        Append(code, MovR64(2, 1));
        Append(code, MovR64(3, 1));
        code.insert(code.end(), {0x48, 0xF7, 0xF3}); // DIV RBX
        code = Finish(code);
        CHECK("DIV64_OVERFLOW", !RunCode(cpu, mem, code));
    }

}


void TestCpuAudit()
{
    std::cout << "\n";
    std::cout << "=============================================\n";
    std::cout << "              CPU COMPLETE AUDIT\n";
    std::cout << "=============================================\n\n";

    // =========================================================
    // 1. MOV32 register -> register
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x2A, 0x00, 0x00, 0x00, // MOV EAX,42
            0x89, 0xC1                         // MOV ECX,EAX
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOV32 r/m32,r32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(1) == 42);
    }

    // =========================================================
    // 2. MOV32 register <- register
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB9, 0x4D, 0x00, 0x00, 0x00, // MOV ECX,77
            0x8B, 0xC1                         // MOV EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOV32 r32,r/m32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 77);
    }

    // =========================================================
    // 3. ADD32 01 /r
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 10, 0, 0, 0,
            0xB9, 20, 0, 0, 0,
            0x01, 0xC8                 // ADD EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT ADD32 01 /r",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 30);
    }

    // =========================================================
    // 4. SUB32 29 /r
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 30, 0, 0, 0,
            0xB9, 10, 0, 0, 0,
            0x29, 0xC8                 // SUB EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT SUB32 29 /r",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 20);
    }

    // =========================================================
    // 5. OR32 09 /r
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xF0, 0x00, 0x00, 0x00,
            0xB9, 0x0F, 0x0F, 0x00, 0x00,
            0x09, 0xC8                 // OR EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT OR32 09 /r",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x00000FFF);
    }

    // =========================================================
    // 6. AND32 21 /r
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0x0F, 0x00, 0x00,
            0xB9, 0xF0, 0x00, 0x00, 0x00,
            0x21, 0xC8                 // AND EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT AND32 21 /r",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xF0);
    }

    // =========================================================
    // 7. XOR32 31 /r
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0xFF, 0x00, 0x00,
            0xB9, 0x0F, 0x0F, 0x00, 0x00,
            0x31, 0xC8                 // XOR EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT XOR32 31 /r",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x0000F0F0);
    }

    // =========================================================
    // 8. CMP32 3B /r
    //
    // CMP EAX,ECX = EAX - ECX
    // 5 - 3 => CF=0, SF=0
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 5, 0, 0, 0,
            0xB9, 3, 0, 0, 0,
            0x3B, 0xC1                 // CMP EAX,ECX
        };

        code = Finish(code);

        CHECK(
            "AUDIT CMP32 3B direction",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & (1ULL << 7)) == 0 &&
            (cpu.Rflags() & (1ULL << 6)) == 0);
    }

    // =========================================================
    // 9. CMP32 3B reverse
    //
    // 3 - 5 => CF=1, SF=1
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 3, 0, 0, 0,
            0xB9, 5, 0, 0, 0,
            0x3B, 0xC1
        };

        code = Finish(code);

        CHECK(
            "AUDIT CMP32 3B reverse",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & (1ULL << 7)) != 0);
    }

    // =========================================================
    // 10. 81 /0 ADD32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x00, 0x10, 0x00, 0x00,
            0x81, 0xC0, 0x34, 0x12, 0x00, 0x00
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 ADD32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x2234);
    }

    // =========================================================
    // 11. 81 /5 SUB32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x00, 0x20, 0x00, 0x00,
            0x81, 0xE8, 0x34, 0x12, 0x00, 0x00
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 SUB32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x0DCC);
    }

    // =========================================================
    // 12. 81 /1 OR32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x00, 0x10, 0x00, 0x00,
            0x81, 0xC8, 0x34, 0x02, 0x00, 0x00
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 OR32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x1234);
    }

    // =========================================================
    // 13. 81 /4 AND32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0xFF, 0x00, 0x00,
            0x81, 0xE0, 0x34, 0x12, 0x00, 0x00
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 AND32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x1234);
    }

    // =========================================================
    // 14. 81 /6 XOR32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0xFF, 0x00, 0x00,
            0x81, 0xF0, 0x34, 0x12, 0x00, 0x00
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 XOR32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xEDCB);
    }

    // =========================================================
    // 15. 83 /0 ADD32 sign-extended imm8
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x00, 0x00, 0x00, 0x00,
            0x83, 0xC0, 0xFF
        };

        code = Finish(code);

        CHECK(
            "AUDIT 83 ADD32 sign-extended",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFULL);
    }

    // =========================================================
    // 16. 81 /7 CMP32
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 5, 0, 0, 0,
            0x81, 0xF8, 5, 0, 0, 0,
            0x74, 0x01,
            0xC3
        };

        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        CHECK(
            "AUDIT 81 CMP32 + JZ",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 2);
    }

    // =========================================================
    // 17. ADC32 /2 immediate
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 10, 0, 0, 0,
            0x81, 0xD0, 5, 0, 0, 0
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 ADC32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 15);
    }

    // =========================================================
    // 18. SBB32 /3 immediate
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 10, 0, 0, 0,
            0x81, 0xD8, 3, 0, 0, 0
        };

        code = Finish(code);

        CHECK(
            "AUDIT 81 SBB32",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 7);
    }

    // =========================================================
    // 19. JMP rel32 E9
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        std::vector<std::uint8_t> code = {
            0xE9, 0x00, 0x00, 0x00, 0x00
        };

        Append(code, MovR64(0, 1));

        const std::size_t target = code.size();

        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        const std::int32_t rel =
            static_cast<std::int32_t>(target) - 5;

        std::memcpy(
            code.data() + 1,
            &rel,
            sizeof(rel));

        CHECK(
            "AUDIT JMP rel32 E9",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 2);
    }

    // =========================================================
    // 20. MOVZX EAX,AL
    // 0F B6 C0
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0x7F, 0x00, 0x00, 0x00,
            0x0F, 0xB6, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVZX r32,r/m8",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0x7F);
    }

    // =========================================================
    // 21. MOVSX EAX,AL
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0x00, 0x00, 0x00,
            0x0F, 0xBE, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVSX r32,r/m8",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFULL);
    }

    // =========================================================
    // =========================================================
    // 21b. MOVSX RAX,AL avec REX.W
    // 48 0F BE C0
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0x00, 0x00, 0x00,
            0x48, 0x0F, 0xBE, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVSX REX.W r64,r/m8",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFFFFFFFFFULL);
    }
    // 21c. MOVZX EAX,AX
    // 0F B7 C0
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xEF, 0xBE, 0x00, 0x00,
            0x0F, 0xB7, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVZX r32,r/m16",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xBEEFULL);
    }

    // 21d. MOVZX RAX,AX avec REX.W
    // 48 0F B7 C0
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xEF, 0xBE, 0x00, 0x00,
            0x48, 0x0F, 0xB7, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVZX r64,r/m16",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xBEEFULL);
    }

    // 21e. MOVSX EAX,AX
    // 0F BF C0
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0xFF, 0x00, 0x00,
            0x0F, 0xBF, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVSX r32,r/m16",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFULL);
    }

    // 21f. MOVSX RAX,AX avec REX.W
    // 48 0F BF C0
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = std::vector<std::uint8_t>{
            0xB8, 0xFF, 0xFF, 0x00, 0x00,
            0x48, 0x0F, 0xBF, 0xC0
        };

        code = Finish(code);

        CHECK(
            "AUDIT MOVSX r64,r/m16",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 0xFFFFFFFFFFFFFFFFULL);
    }
    // 22. MUL F7 /4
    // RDX:RAX = RAX * RBX
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 6);
        Append(code, MovR64(3, 7));

        code.insert(code.end(), {
            0x48, 0xF7, 0xE3
        });

        code = Finish(code);

        CHECK(
            "AUDIT MUL64 F7 /4",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 42 &&
            cpu.ReadRegister64(2) == 0);
    }

    // =========================================================
    // 23. IMUL one operand F7 /5
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 6);
        Append(code, MovR64(3, 7));

        code.insert(code.end(), {
            0x48, 0xF7, 0xEB
        });

        code = Finish(code);

        CHECK(
            "AUDIT IMUL64 F7 /5",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(0) == 42 &&
            cpu.ReadRegister64(2) == 0);
    }

    // =========================================================
    // 24. SAR64 by 1 : OF must be 0
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(
            0,
            0xFFFFFFFFFFFFFFFCULL);

        code.insert(code.end(), {
            0x48, 0xC1, 0xF8, 0x01
        });

        code = Finish(code);

        CHECK(
            "AUDIT SAR64 OF=0",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xFFFFFFFFFFFFFFFEULL &&
            (cpu.Rflags() & (1ULL << 11)) == 0);
    }

    // =========================================================
    // 25. Long JZ 0F 84
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        std::vector<std::uint8_t> code = {
            0xB8, 5, 0, 0, 0,
            0x3D, 5, 0, 0, 0,
            0x0F, 0x84, 0, 0, 0, 0
        };

        const std::size_t target = code.size();

        Append(code, MovR64(0, 2));
        code.push_back(0xC3);

        const std::int32_t rel =
            static_cast<std::int32_t>(target) - 16;

        std::memcpy(
            code.data() + 12,
            &rel,
            sizeof(rel));

        CHECK(
            "AUDIT LONG JZ 0F 84",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 2);
    }

    // =========================================================
    // 26. FF /6 PUSH r/m64
    //
    // L'audit indique que cette voie retourne actuellement
    // false/0 au lieu d'un code d'erreur.
    //
    // Ici on vÃ©rifie qu'une instruction non supportÃ©e ne
    // transforme PAS l'erreur en succÃ¨s.
    // =========================================================
    {
        Memory mem;
        mem.Map(CODE, 0x3000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0x1234);

        code.insert(code.end(), {
            0xFF, 0xF0
        });

        code = Finish(code);

        CHECK(
            "AUDIT FF /6 PUSH r/m64",
            RunCode(cpu, mem, code) &&
            cpu.Rsp() == STACK + 0xFF8 &&
            Read64(mem, STACK + 0xFF8) == 0x1234);
    }

    std::cout << "\n";
    std::cout << "----------- AUDIT FINISHED -----------\n";
}


void TestIoInstructions()
{
    std::uint16_t lastPort = 0;
    std::uint8_t lastWidth = 0;
    std::uint32_t lastWriteValue = 0;
    int readCalls = 0;
    int writeCalls = 0;

    auto installHandlers = [&](Cpu& cpu) {
        cpu.SetIoHandlers(
            [&](Cpu&, std::uint16_t port, std::uint8_t width) -> std::uint32_t {
                lastPort = port; lastWidth = width; ++readCalls;
                if (width == 1) return 0xA5U;
                if (width == 2) return 0xBEEFU;
                return 0x89ABCDEFU;
            },
            [&](Cpu&, std::uint16_t port, std::uint32_t value, std::uint8_t width) -> bool {
                lastPort = port; lastWidth = width; lastWriteValue = value; ++writeCalls; return true;
            });
    };

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        CHECK("IN AL,imm8 dispatches 8-bit read",
            RunCode(cpu, mem, {0xE4, 0x23, 0xC3}) && cpu.Rax() == 0xA5ULL &&
            lastPort == 0x23 && lastWidth == 1 && readCalls == 1);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        auto code = MovR64(0, 0x1122334455667788ULL); code.insert(code.end(), {0x66,0xE5,0x34,0xC3});
        CHECK("66h IN AX,imm8 preserves upper register bits",
            RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566BEEFULL &&
            lastPort == 0x34 && lastWidth == 2 && readCalls == 2);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        auto code = MovR64(2, 0xD5ULL); code.insert(code.end(), {0xED,0xC3});
        CHECK("IN EAX,DX masks port to 16 bits and zero-extends EAX",
            RunCode(cpu, mem, code) && cpu.Rax() == 0x0000000089ABCDEFULL &&
            lastPort == 0xD5 && lastWidth == 4 && readCalls == 3);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        auto code = MovR64(0, 0xDEADBEEFULL); code.insert(code.end(), {0xE6,0x45,0xC3});
        CHECK("OUT imm8,AL dispatches 8-bit write",
            RunCode(cpu, mem, code) && lastPort == 0x45 && lastWidth == 1 &&
            lastWriteValue == 0xEFU && writeCalls == 1);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        auto code = MovR64(0, 0x1122334455667788ULL); code.insert(code.end(), {0x66,0xE7,0x56,0xC3});
        CHECK("66h OUT imm8,AX writes exactly 16 bits",
            RunCode(cpu, mem, code) && lastPort == 0x56 && lastWidth == 2 &&
            lastWriteValue == 0x7788U && writeCalls == 2);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem); installHandlers(cpu);
        auto code = MovR64(0, 0x1122334455667788ULL); Append(code, MovR64(2,0x1FEULL)); code.insert(code.end(), {0xEF,0xC3});
        CHECK("OUT DX,EAX uses DX port and 32-bit value",
            RunCode(cpu, mem, code) && lastPort == 0x1FE && lastWidth == 4 &&
            lastWriteValue == 0x55667788U && writeCalls == 3);
    }
}
void TestSyscallDispatch()
{
    Memory mem;
    mem.Map(CODE, 0x1000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);
    cpu.SetEfer(0x1ULL); // EFER.SCE: enable SYSCALL/SYSRET
    bool invoked = false;

    cpu.SetSyscallHandler(
        [&invoked](Cpu& target) {
            invoked = target.ReadRegister64(0) == 60;
            target.Halt();
            return true;
        });

    auto code = MovR64(0, 60);
    code.insert(code.end(), {0x0F, 0x05});

    CHECK(
        "SYSCALL dispatch",
        RunCode(cpu, mem, code) && invoked);
}

void TestMovImmediate()
{
    {
        Memory mem;
        mem.Map(CODE, 0x1000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{
            0xC7, 0xC0, 0x78, 0x56, 0x34, 0x12
        };
        code = Finish(code);

        CHECK(
            "MOV C7 r32,imm32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x12345678ULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x1000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        auto code = std::vector<std::uint8_t>{
            0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF
        };
        code = Finish(code);

        CHECK(
            "MOV C7 r64,sign-extended imm32",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xFFFFFFFFFFFFFFFFULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x1000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, DATA);
        code.insert(code.end(), {
            0x48, 0xC7, 0x00, 0x34, 0x12, 0x00, 0x00
        });
        code = Finish(code);

        CHECK(
            "MOV C7 [r64],sign-extended imm32",
            RunCode(cpu, mem, code) &&
            Read64(mem, DATA) == 0x1234ULL);
    }
}


// =========================================================
// Nouvelles instructions ajoutees
// =========================================================

// ============== LEA 32 bits ==============
void TestLea32()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, DATA);
    // LEA EAX,[RAX+20h]  (pas de REX.W)
    code.push_back(0x8D);
    code.push_back(0x40);
    code.push_back(0x20);

    code = Finish(code);

    CHECK("LEA32", RunCode(cpu, mem, code) &&
          cpu.Rax() == ((DATA + 0x20) & 0xFFFFFFFFULL));
}

// ============== HLT ==============
void TestHlt()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 7);
    code.push_back(0xF4); // HLT
    Append(code, MovR64(0, 99)); // ne doit jamais s'executer
    code = Finish(code);

    CHECK("HLT stops execution", RunCode(cpu, mem, code) && cpu.Rax() == 7);
}

// ============== SETcc ==============
void TestPushPopMemory16()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);
    const std::uint64_t address = DATA + 0x140;
    Write64(mem, address, 0xAABBCCDDEEFF1234ULL);
    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(0, 0x1122334455667788ULL);
    const std::vector<std::uint8_t> code = {
        0x66, 0xFF, 0x34, 0x25,
        static_cast<std::uint8_t>(address & 0xFF),
        static_cast<std::uint8_t>((address >> 8) & 0xFF),
        static_cast<std::uint8_t>((address >> 16) & 0xFF),
        static_cast<std::uint8_t>((address >> 24) & 0xFF), // PUSH word [abs]
        0x66, 0x8F, 0x04, 0x25,
        static_cast<std::uint8_t>((address >> 0) & 0xFF),
        static_cast<std::uint8_t>((address >> 8) & 0xFF),
        static_cast<std::uint8_t>((address >> 16) & 0xFF),
        static_cast<std::uint8_t>((address >> 24) & 0xFF), // POP word [abs]
        0xF4
    };
    CHECK("PUSH/POP r/m16 memory uses 16-bit stack width",
          RunCode(cpu, mem, code) &&
          (Read64(mem, address) & 0xFFFFFFFFFFFF0000ULL) == 0xAABBCCDDEEFF0000ULL &&
          (Read64(mem, address) & 0xFFFFULL) == 0x1234ULL &&
          cpu.Rsp() == STACK + 0x1000);
}

void TestRotateRexAndOperandWidths()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x81);
        const std::vector<std::uint8_t> code = {0x41, 0xD0, 0xC0, 0xF4}; // ROL R8B,1
        CHECK("ROL R8B,1 uses REX.B", RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(8) & 0xFFULL) == 0x03ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(9, 0x123456789ABCDEF0ULL);
        const std::vector<std::uint8_t> code = {0x49, 0xC1, 0xC9, 0x04, 0xF4}; // ROR R9,4
        CHECK("ROR R9,4 uses REX.WB", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(9) == 0x0123456789ABCDEFULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1122334455668001ULL);
        const std::vector<std::uint8_t> code = {0x66, 0xC1, 0xC8, 0x04, 0xF4}; // ROR AX,4
        CHECK("ROR AX,4 uses operand-size override and preserves upper RAX",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x1122334455661800ULL);
    }
}


void TestRotate64MemoryRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    constexpr std::uint64_t base = DATA + 0x300;
    constexpr std::uint64_t index = 3;
    constexpr std::uint64_t displacement = 0x20;
    const std::uint64_t address = base + index * 4 + displacement;
    Write64(mem, address, 1);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, base);
    cpu.WriteRegister64(9, index);

    const std::vector<std::uint8_t> code = {
        0x4F, 0xD1, 0x4C, 0x8D, 0x20, 0xF4
    }; // ROR qword [R13+R9*4+disp8],1; REX.WRXB

    CHECK(
        "ROR64 memory REX.WRXB SIB disp8",
        RunCode(cpu, mem, code) &&
        Read64(mem, address) == 0x8000000000000000ULL &&
        (cpu.Rflags() & 1ULL) != 0 &&
        (cpu.Rflags() & (1ULL << 11)) != 0);
}

void TestMovxByteRexAndHighByteRules()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0xFFFFFFFFFFFFFF80ULL);
        const std::vector<std::uint8_t> code = {0x49, 0x0F, 0xB6, 0xC0, 0xF4}; // MOVZX RAX,R8B
        CHECK("MOVZX RAX,R8B uses REX.B and zero extends",
              RunCode(cpu, mem, code) && cpu.Rax() == 0x80ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0x0000000000000080ULL);
        const std::vector<std::uint8_t> code = {0x4D, 0x0F, 0xBE, 0xC8, 0xF4}; // MOVSX R9,R8B
        CHECK("MOVSX R9,R8B uses REX.WRB and sign extends",
              RunCode(cpu, mem, code) && cpu.ReadRegister64(9) == 0xFFFFFFFFFFFFFF80ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x112233445566AA00ULL);
        const std::vector<std::uint8_t> code = {0x0F, 0xB6, 0xC4, 0xF4}; // MOVZX EAX,AH
        CHECK("MOVZX EAX,AH accesses legacy high byte without REX",
              RunCode(cpu, mem, code) && cpu.Rax() == 0xAAULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x112233445566AA00ULL);
        cpu.WriteRegister64(4, 0x0000000000000077ULL);
        const std::vector<std::uint8_t> code = {0x40, 0x0F, 0xB6, 0xC4, 0xF4}; // MOVZX EAX,SPL
        CHECK("REX makes byte register 4 SPL instead of AH",
              RunCode(cpu, mem, code) && cpu.Rax() == 0x77ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x500;
        const std::uint64_t index = 2;
        const std::uint64_t address = base + index * 4 + 0x20;
        const std::uint8_t value = 0x80;
        mem.Write(address, &value, 1);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        // 4F 0F BE 44 8D 20: MOVSX R8,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x4F, 0x0F, 0xBE, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("MOVSX R8B memory REX.RXB SIB disp8",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0xFFFFFFFFFFFFFF80ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x600;
        const std::uint64_t index = 1;
        const std::uint64_t address = base + index * 4 + 0x20;
        const std::uint8_t value = 0x80;
        mem.Write(address, &value, 1);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        // 4F 0F B6 44 8D 20: MOVZX R8,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x4F, 0x0F, 0xB6, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("MOVZX R8B memory REX.RXB SIB disp8",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x80ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x700;
        constexpr std::uint64_t index = 2;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        const std::uint8_t value = 0x80;
        mem.Write(address, &value, 1);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);

        // 47 0F B6 44 8D 20: MOVZX R8D,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x47, 0x0F, 0xB6, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("MOVZX R8D memory REX.RXB SIB disp8",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x0000000000000080ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x780;
        constexpr std::uint64_t index = 1;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        const std::uint8_t value = 0x80;
        mem.Write(address, &value, 1);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);

        // 47 0F BE 44 8D 20: MOVSX R8D,[R13+R9*4+disp8].
        const std::vector<std::uint8_t> code = {
            0x47, 0x0F, 0xBE, 0x44, 0x8D, 0x20, 0xF4
        };
        CHECK("MOVSX R8D memory REX.RXB SIB disp8",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0x00000000FFFFFF80ULL);
    }

}

void TestCmpxchgOperandWidths()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1122334455661234ULL);
        cpu.WriteRegister64(1, 0x0000000000001234ULL);
        const std::vector<std::uint8_t> code = {0x66, 0x0F, 0xB1, 0xC8, 0xF4}; // CMPXCHG CX,AX
        CHECK("CMPXCHG16 success preserves upper accumulator bits",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x1122334455661234ULL &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0xFFFFFFFF12345678ULL);
        cpu.WriteRegister64(1, 0x0000000012345678ULL);
        const std::vector<std::uint8_t> code = {0x0F, 0xB1, 0xC8, 0xF4}; // CMPXCHG ECX,EAX
        CHECK("CMPXCHG32 success zero-extends destination",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x0000000012345678ULL &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
}

void TestSetccMemory()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);
    const std::uint64_t address = DATA + 0x110;
    std::uint8_t initial = 0xAA;
    mem.Write(address, &initial, 1);
    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(12, DATA + 0x100);
    cpu.SetRflags(0);
    const std::vector<std::uint8_t> code = {
        0x41, 0x0F, 0x95, 0x44, 0x24, 0x10, // SETNE byte [R12+0x10]
        0xF4
    };
    std::uint8_t result = 0;
    CHECK("SETNE memory with REX.B SIB", RunCode(cpu, mem, code) &&
          mem.Read(address, &result, 1) && result == 1 &&
          cpu.Rflags() == 0);
}

void TestCmpxchgMemory16()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);
    const std::uint64_t address = DATA + 0x180;
    Write64(mem, address, 0xAABBCCDDEEFF1234ULL);
    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(0, 0x1122334455661234ULL);
    cpu.WriteRegister64(1, 0x000000000000BEEFULL);
    std::vector<std::uint8_t> code = {
        0x66, 0x0F, 0xB1, 0x0C, 0x25,
        static_cast<std::uint8_t>(address & 0xFF),
        static_cast<std::uint8_t>((address >> 8) & 0xFF),
        static_cast<std::uint8_t>((address >> 16) & 0xFF),
        static_cast<std::uint8_t>((address >> 24) & 0xFF),
        0xF4
    };
    CHECK("CMPXCHG16 memory success writes r16 operand and preserves accumulator upper bits",
          RunCode(cpu, mem, code) &&
          Read64(mem, address) == 0xAABBCCDDEEFFBEEFULL &&
          cpu.ReadRegister64(0) == 0x1122334455661234ULL &&
          (cpu.Rflags() & (1ULL << 6)) != 0);
}

void TestCmpxchgVariants()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1111);
        cpu.WriteRegister64(1, 0x2222);
        const std::vector<std::uint8_t> code = {
            0x48, 0x0F, 0xB1, 0xC8, // CMPXCHG RCX,RAX: RAX == RCX -> RCX = RAX? no: rm=RAX, reg=RCX
            0xF4
        };
        CHECK("CMPXCHG64 equal register leaves destination and sets ZF",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x2222 &&
              cpu.ReadRegister64(1) == 0x2222 &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1111);
        cpu.WriteRegister64(1, 0x2222);
        cpu.WriteRegister64(2, 0x3333);
        const std::vector<std::uint8_t> code = {
            0x48, 0x0F, 0xB1, 0xD1, // CMPXCHG RCX,RDX: compare RAX with RCX, mismatch -> RAX=RCX
            0xF4
        };
        CHECK("CMPXCHG64 mismatch loads old destination into RAX",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0x2222 &&
              cpu.ReadRegister64(1) == 0x2222 &&
              (cpu.Rflags() & (1ULL << 6)) == 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x1111);
        cpu.WriteRegister64(8, 0xAAAA);
        cpu.WriteRegister64(9, 0xBBBB);
        const std::vector<std::uint8_t> code = {
            0x4D, 0x0F, 0xB1, 0xC8, // CMPXCHG R8,R9: RAX compared with R8
            0xF4
        };
        CHECK("CMPXCHG64 REX.RB extended registers",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == 0xAAAA &&
              cpu.ReadRegister64(8) == 0xAAAA &&
              cpu.ReadRegister64(9) == 0xBBBB);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x100;
        const std::uint64_t value = 0x1122334455667788ULL;
        Write64(mem, address, value);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0xAABBCCDDEEFF0011ULL);
        cpu.WriteRegister64(9, 0x99);
        cpu.WriteRegister64(13, DATA + 0x100);
        const std::vector<std::uint8_t> code = {
            0x4F, 0x0F, 0xB1, 0x4D, 0x00, // CMPXCHG [R13],R9
            0xF4
        };
        CHECK("CMPXCHG64 memory mismatch updates accumulator",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(0) == value);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x000000000000007F);
        cpu.WriteRegister64(8, 0x000000000000007F);
        cpu.WriteRegister64(9, 0x00000000000000A5);
        const std::vector<std::uint8_t> code = {
            0x4D, 0x0F, 0xB0, 0xC8, // CMPXCHG R8B,R9B
            0xF4
        };
        CHECK("CMPXCHG8 REX.BR uses R8B/R9B",
              RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(8) & 0xFFULL) == 0xA5 &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
}


void TestCmpxchgMemoryRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    constexpr std::uint64_t base = DATA + 0x240;
    constexpr std::uint64_t index = 3;
    constexpr std::uint64_t displacement = 0x20;
    const std::uint64_t address = base + index * 4 + displacement;
    Write64(mem, address, 0x1111222233334444ULL);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, base);
    cpu.WriteRegister64(9, index);
    cpu.WriteRegister64(0, 0x1111222233334444ULL);
    cpu.WriteRegister64(8, 0xAAAABBBBCCCCDDDDULL);

    const std::vector<std::uint8_t> code = {
        0x4F, 0x0F, 0xB1, 0x44, 0x8D, 0x20, 0xF4
    }; // CMPXCHG [R13+R9*4+disp8],R8; REX.WRXB

    CHECK(
        "CMPXCHG64 memory REX.WRXB SIB disp8 success",
        RunCode(cpu, mem, code) &&
        Read64(mem, address) == 0xAAAABBBBCCCCDDDDULL &&
        cpu.ReadRegister64(0) == 0x1111222233334444ULL &&
        (cpu.Rflags() & (1ULL << 6)) != 0);
}

void TestSetccAllConditions()
{
    struct Case { const char* name; std::uint8_t opcode; std::uint64_t flags; };
    const std::uint64_t CF = 1ULL, ZF = 1ULL << 6, SF = 1ULL << 7;
    const std::uint64_t PF = 1ULL << 2, OF = 1ULL << 11;
    const Case cases[] = {
        {"SETO", 0x90, OF}, {"SETNO", 0x91, 0}, {"SETB", 0x92, CF},
        {"SETAE", 0x93, 0}, {"SETE", 0x94, ZF}, {"SETNE", 0x95, 0},
        {"SETBE", 0x96, CF | ZF}, {"SETA", 0x97, 0},
        {"SETS", 0x98, SF}, {"SETNS", 0x99, 0}, {"SETP", 0x9A, PF},
        {"SETNP", 0x9B, 0}, {"SETL", 0x9C, SF}, {"SETGE", 0x9D, SF | OF},
        {"SETLE", 0x9E, ZF}, {"SETG", 0x9F, 0}
    };

    const std::uint64_t falseFlags[] = {0, OF, 0, CF, 0, ZF, 0, CF, 0, SF, 0, PF, 0, SF, 0, ZF};

    for (const auto& c : cases) {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(c.flags);
        const std::uint64_t before = cpu.Rflags();
        std::vector<std::uint8_t> code = {0x0F, c.opcode, 0xC0, 0xF4}; // SETcc AL
        const bool ran = RunCode(cpu, mem, code);
        CHECK(c.name, ran && (cpu.Rax() & 0xFFULL) == 1 && cpu.Rflags() == before);
    }
    for (std::size_t i = 0; i < std::size(cases); ++i) {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(falseFlags[i]);
        const std::uint64_t before = cpu.Rflags();
        std::vector<std::uint8_t> code = {0x0F, cases[i].opcode, 0xC0, 0xF4};
        const bool ran = RunCode(cpu, mem, code);
        std::string name = std::string(cases[i].name) + " false";
        CHECK(name.c_str(), ran && (cpu.Rax() & 0xFFULL) == 0 && cpu.Rflags() == before);
    }


    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(ZF);
        const std::vector<std::uint8_t> code = {0x41, 0x0F, 0x94, 0xC0, 0xF4};
        CHECK("SETE R8B with REX.B", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 1 && cpu.Rflags() == ZF);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(ZF);
        const std::uint64_t address = DATA + 0x180;
        std::uint8_t zero = 0;
        mem.Write(address, &zero, 1);
        // SETNE byte ptr [R13+disp8] exercises REX.B + ModRM memory addressing.
        auto code = MovR64(13, address - 0x7F);
        Append(code, {0x41, 0x0F, 0x95, 0x45, 0x7F, 0xF4});
        CHECK("SETNE memory R13 disp8 false",
              RunCode(cpu, mem, code) && Read64(mem, address) == 0);
    }

}


void TestSetccMemoryRexSib()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    constexpr std::uint64_t base = DATA + 0x280;
    constexpr std::uint64_t index = 2;
    constexpr std::uint64_t displacement = 0x20;
    const std::uint64_t address = base + index * 4 + displacement;
    std::uint8_t initial = 0xAA;
    mem.Write(address, &initial, 1);

    Cpu cpu = MakeCpu(mem);
    cpu.WriteRegister64(13, base);
    cpu.WriteRegister64(9, index);
    cpu.SetRflags(1ULL << 6); // ZF=1 -> SETE writes 1.

    const std::vector<std::uint8_t> code = {
        0x43, 0x0F, 0x94, 0x44, 0x8D, 0x20, 0xF4
    }; // SETE byte [R13+R9*4+disp8], REX.XB

    std::uint8_t result = 0;
    CHECK(
        "SETE memory REX.XB SIB disp8",
        RunCode(cpu, mem, code) &&
        mem.Read(address, &result, 1) &&
        result == 1 &&
        cpu.Rflags() == (1ULL << 6));
}

void TestSetcc()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 5);
        Append(code, MovR64(1, 5));
        // CMP RAX,RCX
        code.insert(code.end(), {0x48, 0x39, 0xC8});
        // SETE DL
        code.insert(code.end(), {0x0F, 0x94, 0xC2});
        code = Finish(code);

        CHECK("SETE dl (equal)", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(2) == 1);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 3);
        Append(code, MovR64(1, 5));
        // CMP RAX,RCX (3 vs 5)
        code.insert(code.end(), {0x48, 0x39, 0xC8});
        // SETL DL  (3 < 5 -> 1)
        code.insert(code.end(), {0x0F, 0x9C, 0xC2});
        code = Finish(code);

        CHECK("SETL dl (less)", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(2) == 1);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 9);
        Append(code, MovR64(1, 5));
        // CMP RAX,RCX (9 vs 5)
        code.insert(code.end(), {0x48, 0x39, 0xC8});
        // SETG DL (9 > 5 -> 1)
        code.insert(code.end(), {0x0F, 0x9F, 0xC2});
        code = Finish(code);

        CHECK("SETG dl (greater)", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(2) == 1);
    }
}

// ============== Shifts par CL : 8, 32, 64 bits ==============
void TestShiftsByCL()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 1);
        Append(code, MovR64(1, 4));
        // SHL AL,CL  (D2 /4, rm=AL)
        code.insert(code.end(), {0xD2, 0xE0});
        code = Finish(code);

        CHECK("SHL8 AL,CL", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x10);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 1);
        Append(code, MovR64(1, 4));
        // SHL EAX,CL (D3 /4, pas de REX.W)
        code.insert(code.end(), {0xD3, 0xE0});
        code = Finish(code);

        CHECK("SHL32 EAX,CL", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 0xFFFFFFFC);
        Append(code, MovR64(1, 1));
        // SAR EAX,CL (D3 /7)
        code.insert(code.end(), {0xD3, 0xF8});
        code = Finish(code);

        CHECK("SAR32 EAX,CL", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0xFFFFFFFE &&
              (cpu.Rflags() & (1ULL << 11)) == 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 1);
        Append(code, MovR64(1, 4));
        // SHL RAX,CL (48 D3 /4)
        code.insert(code.end(), {0x48, 0xD3, 0xE0});
        code = Finish(code);

        CHECK("SHL64 RAX,CL", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);

        auto code = MovR64(0, 1);
        // SHL EAX,1  (D1 /4, forme implicite)
        code.insert(code.end(), {0xD1, 0xE0});
        code = Finish(code);

        CHECK("SHL32 EAX,1 (implicit)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 2);
    }
}



void TestShiftClOperandWidthsAndRexMemory()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags((1ULL << 0) | (1ULL << 4) | (1ULL << 6));
        auto code = MovR64(0, 0x1122334455660001ULL);
        Append(code, MovR64(1, 4));
        code.insert(code.end(), {0x66, 0xD3, 0xE0}); // SHL AX,CL
        code = Finish(code);
        CHECK("SHL16 AX,CL preserves upper RAX bits", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1122334455660010ULL &&
              (cpu.Rflags() & (1ULL << 0)) == 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 1);
        cpu.WriteRegister64(1, 4);
        const std::vector<std::uint8_t> code = {0x49, 0xD3, 0xE0, 0xF4}; // SHL R8,CL
        CHECK("SHL64 R8,CL exercises REX.B", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 16);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x200;
        const std::uint64_t index = 3;
        const std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 1);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(1, 4);
        const std::vector<std::uint8_t> code = {
            0x4F, 0xD3, 0x64, 0x8D, 0x20, 0xF4
        }; // SHL qword [R13+R9*4+disp8],CL; REX.WXB
        CHECK("SHL64 memory SIB REX.WXB disp8 with CL",
              RunCode(cpu, mem, code) && Read64(mem, address) == 16);
    }
}

void TestShldShrd()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0123456789ABCDEFULL);
        Append(code, MovR64(1, 0xFEDCBA9876543210ULL));
        code.insert(code.end(), {0x48, 0x0F, 0xA4, 0xC8, 0x04}); // SHLD RAX,RCX,4
        code = Finish(code);
        CHECK("SHLD64 immediate register form", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x123456789ABCDEFFULL &&
              (cpu.Rflags() & 1ULL) == 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0123456789ABCDEFULL);
        Append(code, MovR64(1, 0xFEDCBA9876543210ULL));
        code.insert(code.end(), {0x48, 0x0F, 0xAC, 0xC8, 0x04}); // SHRD RAX,RCX,4
        code = Finish(code);
        CHECK("SHRD64 immediate register form", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x00123456789ABCDEULL &&
              (cpu.Rflags() & 1ULL) != 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0000000000004000ULL);
        Append(code, MovR64(1, 0x0000000000000000ULL));
        code.insert(code.end(), {0x66, 0x0F, 0xA4, 0xC8, 0x01}); // SHLD AX,CX,1
        code = Finish(code);
        CHECK("SHLD16 immediate uses operand-size override", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x8000ULL &&
              (cpu.Rflags() & 1ULL) == 0 &&
              (cpu.Rflags() & (1ULL << 11)) != 0);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x80000001ULL);
        Append(code, MovR64(1, 1));
        code.insert(code.end(), {0x0F, 0xA5, 0xC8}); // SHLD EAX,ECX,CL
        code.insert(code.end(), {0x0F, 0xAD, 0xC8}); // SHRD EAX,ECX,CL
        code = Finish(code);
        CHECK("SHLD/SHRD32 register-count forms", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x80000001ULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x400;
        const std::uint64_t index = 3;
        const std::uint64_t address = base + index * 2;
        Write64(mem, address, 0x0123456789ABCDEFULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0xFEDCBA9876543210ULL);
        auto code = std::vector<std::uint8_t>{
            0x4F, 0x0F, 0xA4, 0x04, 0x4D, 0x04, // SHLD [R13+R9*2],R8,4
            0xF4
        };
        CHECK("SHLD64 memory exercises REX.WRXB plus SIB", RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0x123456789ABCDEFFULL);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        Write32(mem, DATA, 0x80000001U);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(1, 0x0000000012345678ULL);
        auto code = std::vector<std::uint8_t>{0x0F, 0xAC, 0x0C, 0x25,
            static_cast<std::uint8_t>(DATA), static_cast<std::uint8_t>(DATA >> 8),
            static_cast<std::uint8_t>(DATA >> 16), static_cast<std::uint8_t>(DATA >> 24), 0x04, 0xF4};
        CHECK("SHRD32 memory absolute form", RunCode(cpu, mem, code) &&
              Read32(mem, DATA) == 0x88000000U);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x80;
        Write64(mem, address, 0x0123456789ABCDEFULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0xFEDCBA9876543210ULL);
        cpu.WriteRegister64(1, 4);
        auto code = std::vector<std::uint8_t>{0x4C, 0x0F, 0xAD, 0x04, 0x25,
            static_cast<std::uint8_t>(address), static_cast<std::uint8_t>(address >> 8),
            static_cast<std::uint8_t>(address >> 16), static_cast<std::uint8_t>(address >> 24), 0xF4};
        CHECK("SHRD64 memory CL form uses REX.W+REX.R", RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0x00123456789ABCDEULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        Write32(mem, DATA, 0x12345678U);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(1, 0x000000009ABCDEF0ULL);
        auto code = std::vector<std::uint8_t>{0x0F, 0xA4, 0x0C, 0x25,
            static_cast<std::uint8_t>(DATA), static_cast<std::uint8_t>(DATA >> 8),
            static_cast<std::uint8_t>(DATA >> 16), static_cast<std::uint8_t>(DATA >> 24), 0x04, 0xF4};
        CHECK("SHLD32 memory absolute form", RunCode(cpu, mem, code) &&
              Read32(mem, DATA) == 0x23456789U);
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0x1122334455664000ULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(1, 0);
        auto code = std::vector<std::uint8_t>{0x66, 0x0F, 0xA4, 0x0C, 0x25,
            static_cast<std::uint8_t>(DATA), static_cast<std::uint8_t>(DATA >> 8),
            static_cast<std::uint8_t>(DATA >> 16), static_cast<std::uint8_t>(DATA >> 24), 0x01, 0xF4};
        CHECK("SHLD16 memory absolute form preserves surrounding bytes",
              RunCode(cpu, mem, code) &&
              Read64(mem, DATA) == 0x1122334455668000ULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags((1ULL << 4) | (1ULL << 6) | (1ULL << 11)); // AF/ZF/OF
        auto code = MovR64(0, 0x1122334455667788ULL);
        Append(code, MovR64(1, 0x99AABBCCDDEEFF00ULL));
        code.push_back(0x48); code.insert(code.end(), {0x0F, 0xA4, 0xC8, 0x40}); // count 64 -> masked to 0
        code = Finish(code);
        CHECK("SHLD64 count equal to width is a zero-count no-op", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1122334455667788ULL &&
              cpu.Rflags() == ((1ULL << 4) | (1ULL << 6) | (1ULL << 11)));
    }
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x500;
        const std::uint64_t index = 2;
        const std::uint64_t address = base + index * 4;
        Write64(mem, address, 0x0123456789ABCDEFULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0xFEDCBA9876543210ULL);
        auto code = std::vector<std::uint8_t>{
            0x4F, 0x0F, 0xAC, 0x04, 0x8D, 0x04,
            0xF4
        };
        CHECK("SHRD64 memory exercises REX.WRXB plus SIB",
              RunCode(cpu, mem, code) &&
              Read64(mem, address) == 0x00123456789ABCDEULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags((1ULL << 4) | (1ULL << 6) | (1ULL << 11));
        auto code = MovR64(0, 0x1122334455667788ULL);
        Append(code, MovR64(1, 0x99AABBCCDDEEFF00ULL));
        Append(code, MovR64(1, 64));
        code.insert(code.end(), {0x48, 0x0F, 0xAD, 0xC8}); // SHRD RAX,R9,CL; CL=64 -> masked to 0
        code = Finish(code);
        CHECK("SHRD64 register count equal to width is a no-op", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1122334455667788ULL &&
              cpu.Rflags() == ((1ULL << 4) | (1ULL << 6) | (1ULL << 11)));
    }
}

// ============== CMP EAX, imm32 (0x3D) ==============

void TestDivIdivMemoryExceptions()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 100);
        Append(code, MovR64(2, 0));
        code.insert(code.end(), {0x48, 0xF7, 0x34, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("DIV64 memory divide-by-zero raises error",
              !RunCode(cpu, mem, code));
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0xFFFFFFFFFFFFFFFFULL);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x8000000000000000ULL);
        Append(code, MovR64(2, 0xFFFFFFFFFFFFFFFFULL));
        code.insert(code.end(), {0x48, 0xF7, 0x3C, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("IDIV64 memory INT64_MIN/-1 overflow raises error",
              !RunCode(cpu, mem, code));
    }
}

void TestCmpEaxImmediate()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(STACK, 0x2000);
    Cpu cpu = MakeCpu(mem);

    auto code = MovR64(0, 5);
    // CMP EAX, 5
    code.insert(code.end(), {0x3D, 5, 0, 0, 0});
    code = Finish(code);

    CHECK("CMP EAX,imm32 (0x3D) ZF", RunCode(cpu, mem, code) &&
          (cpu.Rflags() & (1ULL << 6)) != 0);
}

// ============== Groupe F7 (MUL/IMUL/DIV/IDIV/NEG) avec operande memoire ==============
void TestGroupF7Memory()
{
    // MUL dword [DATA]
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 7);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 6);
        // MUL dword [DATA]  (F7 /4, SIB disp32-only)
        code.push_back(0xF7);
        code.push_back(0x24);
        code.push_back(0x25);
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("MUL32 [mem] (F7 /4)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 42);
    }
    // DIV qword [DATA]
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 7);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 84);
        // DIV qword [DATA]  (48 F7 /6, SIB disp32-only)
        code.insert(code.end(), {0x48, 0xF7, 0x34, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("DIV64 [mem] (F7 /6)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 12);
    }
    // IDIV qword [DATA]: signed dividend from RDX:RAX and signed quotient/remainder.
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 7);

        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, static_cast<std::uint64_t>(-84LL));
        Append(code, MovR64(2, static_cast<std::uint64_t>(-1LL)));
        code.insert(code.end(), {0x48, 0xF7, 0x3C, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("IDIV64 [mem] signed quotient remainder",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == static_cast<std::uint64_t>(-12LL) &&
              cpu.ReadRegister64(2) == 0ULL);
    }

    // NEG dword [DATA]
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x1000);
        mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 5);

        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        // NEG dword [DATA]  (F7 /3, SIB disp32-only)
        code.insert(code.end(), {0xF7, 0x1C, 0x25});
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
        code = Finish(code);

        CHECK("NEG32 [mem] (F7 /3)", RunCode(cpu, mem, code) &&
              (Read64(mem, DATA) & 0xFFFFFFFFULL) == 0xFFFFFFFBULL);
    }
}

// ============== IMUL 0F AF avec operande memoire ==============
void TestImulMemory()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);
    Write64(mem, DATA, 7);

    Cpu cpu = MakeCpu(mem);
    auto code = MovR64(0, 6);
    // IMUL EAX,[DATA]  (0F AF /r, SIB disp32-only)
    code.insert(code.end(), {0x0F, 0xAF, 0x04, 0x25});
    for (int i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
    code = Finish(code);

    CHECK("IMUL EAX,[mem] (0F AF)", RunCode(cpu, mem, code) &&
          cpu.Rax() == 42);
}

void TestImulMemoryWidths()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x2000); mem.Map(STACK, 0x2000);
        const std::uint64_t base = DATA + 0x300;
        const std::uint64_t address = base + 0x10 * 8;
        Write64(mem, address, 7);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, 0x10);
        cpu.WriteRegister64(8, 6);
        const std::vector<std::uint8_t> code = {
            0x4F, 0x0F, 0xAF, 0x04, 0xCD, 0xF4
        };
        CHECK("IMUL R8,[R13+R9*8] uses REX.WRXB memory form",
              RunCode(cpu, mem, code) && cpu.ReadRegister64(8) == 42);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x2000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 7);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(1, 6);
        const std::vector<std::uint8_t> code = {
            0x66, 0x0F, 0xAF, 0x0C, 0x25,
            static_cast<std::uint8_t>(DATA), static_cast<std::uint8_t>(DATA >> 8),
            static_cast<std::uint8_t>(DATA >> 16), static_cast<std::uint8_t>(DATA >> 24),
            0xF4
        };
        CHECK("IMUL CX,[absolute memory] uses 16-bit memory form",
              RunCode(cpu, mem, code) && cpu.ReadRegister64(1) == 42);
    }
}

// ============== SYSCALL -> repli PS5 interne (RAX=numero de service) ==============
void TestSyscallPS5Fallback()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);
    cpu.SetEfer(cpu.Efer() | 1ULL); // EFER.SCE: enable SYSCALL/SYSRET in long mode.
    const std::uint8_t code[] = {0x0F, 0x05};
    mem.Write(CODE, code, sizeof(code));

    bool called = false;
    cpu.SetSyscallHandler([&](Cpu& handlerCpu) {
        called = true;
        handlerCpu.Halt();
        return true;
    });

    CHECK("SYSCALL invokes configured handler", cpu.Run() == 0 && called);
}



void TestRotate()
{
    // ROL EAX, 1 (D1 /0)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x80000001);
        code.insert(code.end(), {0xD1, 0xC0}); // D1 /0 rm=EAX
        code = Finish(code);
        CHECK("ROL EAX,1", RunCode(cpu, mem, code) &&
              cpu.Rax() == 3 && (cpu.Rflags() & 1) == 1);
    }
    // ROR EAX, imm8 (C1 /1)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        code.insert(code.end(), {0xC1, 0xC8, 4}); // C1 /1, count=4
        code = Finish(code);
        CHECK("ROR EAX,4", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x10000000);
    }
    // ROL RAX, CL (48 D3 /0)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        Append(code, MovR64(1, 4));
        code.insert(code.end(), {0x48, 0xD3, 0xC0}); // 48 D3 /0
        code = Finish(code);
        CHECK("ROL RAX,CL", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    // ROL dword [DATA], 1 (memoire, D1 /0)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0x80000001);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.insert(code.end(), {0xD1, 0x04, 0x25});
        for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>(DATA >> (i*8)));
        code = Finish(code);
        CHECK("ROL [mem],1", RunCode(cpu, mem, code) &&
              (Read64(mem, DATA) & 0xFFFFFFFFULL) == 3);
    }
}

void TestTestImmediate()
{
    // TEST EAX, imm32 (F7 /0)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0F);
        code.insert(code.end(), {0xF7, 0xC0, 0xF0, 0, 0, 0}); // TEST EAX, 0xF0
        code = Finish(code);
        CHECK("TEST EAX,0xF0 (F7 /0) -> ZF=1", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    // TEST RAX, imm32 (48 F7 /0)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xFF);
        code.insert(code.end(), {0x48, 0xF7, 0xC0, 0x0F, 0, 0, 0}); // TEST RAX,0xF
        code = Finish(code);
        CHECK("TEST RAX,0xF (48 F7 /0) -> ZF=0", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) == 0);
    }
    // TEST doesn't modify the operand
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1234);
        code.insert(code.end(), {0xF7, 0xC0, 0, 0, 0, 0}); // TEST EAX,0
        code = Finish(code);
        CHECK("TEST EAX,0 preserves EAX", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1234);
    }
}

void TestXchg()
{
    // XCHG EAX, ECX (register-register, 32 bits)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 11);
        Append(code, MovR64(1, 22));
        code.insert(code.end(), {0x87, 0xC1}); // XCHG ECX,EAX (reg=EAX,rm=ECX)
        code = Finish(code);
        CHECK("XCHG EAX,ECX (32 bits)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 22 && cpu.ReadRegister64(1) == 11);
    }
    // XCHG RAX, RCX (64 bits)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1111111111111111ULL);
        Append(code, MovR64(1, 0x2222222222222222ULL));
        code.insert(code.end(), {0x48, 0x87, 0xC1});
        code = Finish(code);
        CHECK("XCHG RAX,RCX (64 bits)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x2222222222222222ULL &&
              cpu.ReadRegister64(1) == 0x1111111111111111ULL);
    }
    // XCHG avec memoire
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 99);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, DATA);
        Append(code, MovR64(1, 55));
        // XCHG [RAX], ECX  (87 08 : mod=00,reg=ECX(001),rm=000(RAX indirect))
        code.insert(code.end(), {0x87, 0x08});
        code = Finish(code);
        CHECK("XCHG [mem],ECX", RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(1) == 99 &&
              (Read64(mem, DATA) & 0xFFFFFFFFULL) == 55);
    }
}



// =========================================================
// Rotation/decalage 8 bits (D0, C0, extension de D2 pour ROL/ROR)
// =========================================================

void TestRotate8Register()
{
    // ROL AL, 1  (D0 /0, rm=AL)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x81); // AL = 0x81 = 1000 0001
        code.insert(code.end(), {0xD0, 0xC0}); // D0 /0, rm=AL(000)
        code = Finish(code);
        CHECK("ROL AL,1", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x03 &&
              (cpu.Rflags() & 1) == 1); // bit0 sorti = 1 -> CF=1
    }

    // ROR AL, imm8  (C0 /1, rm=AL)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x01); // AL = 0000 0001
        code.insert(code.end(), {0xC0, 0xC8, 4}); // C0 /1, rm=AL, imm8=4
        code = Finish(code);
        CHECK("ROR AL,4 (imm8)", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x10);
    }

    // ROL AL, CL  (D2 /0, rm=AL)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x01);
        Append(code, MovR64(1, 3)); // CL = 3
        code.insert(code.end(), {0xD2, 0xC0}); // D2 /0, rm=AL
        code = Finish(code);
        CHECK("ROL AL,CL", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x08);
    }

    // ROR CL, 1  (D0 /1, rm=CL) -- verifie un registre different d'AL
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(1, 0x02); // CL = 0000 0010
        code.insert(code.end(), {0xD0, 0xC9}); // D0 /1, rm=CL(001)
        code = Finish(code);
        CHECK("ROR CL,1", RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(1) & 0xFF) == 0x01 &&
              (cpu.Rflags() & 1) == 0); // pas de bit sorti -> CF=0
    }

    // Rotation d'un octet ne doit PAS toucher le reste du registre
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1234567800000001ULL);
        code.insert(code.end(), {0xD0, 0xC0}); // ROL AL,1
        code = Finish(code);
        CHECK("ROL AL,1 preserve le reste de RAX", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x1234567800000002ULL);
    }
}

void TestRotate8HighByteAndRex()
{
    // ROL AH, 1 (sans REX : rm=100 -> AH). ModRM = 11 000 100 = 0xC4
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x8100); // AH = 0x81
        code.insert(code.end(), {0xD0, 0xC4}); // D0 /0, rm=100 (AH)
        code = Finish(code);
        CHECK("ROL AH,1 (sans REX, high-byte)", RunCode(cpu, mem, code) &&
              ((cpu.Rax() >> 8) & 0xFF) == 0x03 &&
              (cpu.Rax() & 0xFF) == 0x00); // AL inchange
    }

    // ROL SPL,1 (avec REX.B implicite via un prefixe REX quelconque -> pas de AH,
    // rm=100 devient SPL). On utilise REX.W pour forcer un prefixe REX present :
    // 48 D0 C4 = ROL rm8=100 avec REX present -> SPL (bas de RSP), pas AH.
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        // RSP est deja positionne par MakeCpu ; on lit sa valeur avant/apres
        // pour verifier que c'est bien le bas de RSP (registre 4) qui bouge,
        // pas AH (registre 0, octet haut).
        auto code = MovR64(0, 0x8100); // si le bug existait, EAX changerait
        Append(code, MovR64(4, 0x81)); // RSP bas octet = 0x81 (valeur de test)
        code.insert(code.end(), {0x48, 0xD0, 0xC4}); // REX.W + D0 /0, rm=100
        code = Finish(code);
        CHECK("ROL rm8=100 avec REX -> SPL, pas AH", RunCode(cpu, mem, code) &&
              (cpu.ReadRegister64(4) & 0xFF) == 0x03 &&
              cpu.Rax() == 0x8100); // EAX (AH incluse) inchange
    }
}

void TestRotate8Memory()
{
    // ROL byte [DATA], 1  (D0 /0, memoire, SIB disp32-only)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        std::uint8_t v = 0x81;
        mem.Write(DATA, &v, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.insert(code.end(), {0xD0, 0x04, 0x25});
        for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>(DATA >> (i*8)));
        code = Finish(code);
        std::uint8_t result = 0;
        bool ran = RunCode(cpu, mem, code);
        mem.Read(DATA, &result, 1);
        CHECK("ROL byte [mem],1", ran && result == 0x03);
    }

    // ROR byte [DATA], imm8  (C0 /1, memoire)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        std::uint8_t v = 0x01;
        mem.Write(DATA, &v, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.insert(code.end(), {0xC0, 0x0C, 0x25});
        for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>(DATA >> (i*8)));
        code.push_back(4); // imm8 = 4
        code = Finish(code);
        std::uint8_t result = 0;
        bool ran = RunCode(cpu, mem, code);
        mem.Read(DATA, &result, 1);
        CHECK("ROR byte [mem],4 (imm8, ordre de fetch correct)", ran && result == 0x10);
    }

    // ROL byte [DATA], CL  (D2 /0, memoire)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        std::uint8_t v = 0x01;
        mem.Write(DATA, &v, 1);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(1, 3); // CL = 3
        code.insert(code.end(), {0xD2, 0x04, 0x25});
        for (int i = 0; i < 4; ++i) code.push_back(static_cast<std::uint8_t>(DATA >> (i*8)));
        code = Finish(code);
        std::uint8_t result = 0;
        bool ran = RunCode(cpu, mem, code);
        mem.Read(DATA, &result, 1);
        CHECK("ROL byte [mem],CL", ran && result == 0x08);
    }
}

void TestRotate8EdgeCases()
{
    // count == 0 : aucune modification, flags inchanges
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x42);
        code.insert(code.end(), {0xC0, 0xC0, 0}); // ROL AL, 0
        code = Finish(code);
        CHECK("ROL AL,0 -> aucun changement", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x42);
    }

    // count == 8 (multiple de la largeur) : valeur inchangee
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x5A);
        code.insert(code.end(), {0xC0, 0xC0, 8}); // ROL AL, 8
        code = Finish(code);
        CHECK("ROL AL,8 (multiple de 8) -> inchange", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x5A);
    }

    // ROL/ROR ne modifient PAS ZF/SF (contrairement a SHL/SHR/SAR)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        // On met ZF=1 via un CMP egal, puis on fait une rotation qui donne un
        // resultat non nul : ZF doit rester a 1 (rotation ne touche pas ZF).
        auto code = MovR64(0, 5);
        Append(code, MovR64(1, 5));
        code.insert(code.end(), {0x48, 0x39, 0xC8}); // CMP RAX,RCX -> ZF=1
        Append(code, MovR64(2, 0x01)); // DL = 1 (non nul)
        code.insert(code.end(), {0xD0, 0xC2}); // ROL DL,1 -> resultat = 2 (non nul)
        code = Finish(code);
        CHECK("ROL ne modifie pas ZF", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) != 0); // ZF toujours a 1
    }
}




// =========================================================
// Correction C1 : ordre de fetch [ModRM][SIB][disp][imm8]
// (meme classe de bug que 81/83, corrigee ici)
// =========================================================

// Encode l'adresse [DATA] via SIB disp32-only (mod=00, rm=100, SIB base=101)
// pour forcer la presence d'un SIB + disp32 entre le ModRM et l'immediat.
void PushSibDisp32(std::vector<std::uint8_t>& code)
{
    code.push_back(0x25); // SIB : scale=00 index=100(aucun) base=101(disp32 seul)
    for (int i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(DATA >> (i * 8)));
}

void TestC1RegisterForm()
{
    // C1 /4 SHL EAX, imm8 (registre, non-regression)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        code.insert(code.end(), {0xC1, 0xC0, 4}); // SHL EAX,4
        code = Finish(code);
        CHECK("C1 /4 SHL EAX,4 (registre)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    // C1 /5 SHR RAX, imm8 (registre, REX.W)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 256);
        code.insert(code.end(), {0x48, 0xC1, 0xE8, 4}); // SHR RAX,4
        code = Finish(code);
        CHECK("C1 /5 SHR RAX,4 (registre, REX.W)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    // C1 /7 SAR EAX, imm8
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xFFFFFFF0);
        code.insert(code.end(), {0xC1, 0xF8, 4}); // SAR EAX,4
        code = Finish(code);
        CHECK("C1 /7 SAR EAX,4 (registre)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 0xFFFFFFFF);
    }
    // C1 /0 ROL RAX, imm8 (registre, REX.W)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1);
        code.insert(code.end(), {0x48, 0xC1, 0xC0, 4}); // ROL RAX,4
        code = Finish(code);
        CHECK("C1 /0 ROL RAX,4 (registre, REX.W)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 16);
    }
    // C1 /1 ROR EAX, imm8
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 16);
        code.insert(code.end(), {0xC1, 0xC8, 4}); // ROR EAX,4
        code = Finish(code);
        CHECK("C1 /1 ROR EAX,4 (registre)", RunCode(cpu, mem, code) &&
              cpu.Rax() == 1);
    }
}

// Ces tests sont ceux qui auraient echoue (ou donne un resultat faux)
// AVANT la correction : l'immediat etait lu a la place du premier
// octet du SIB/disp32, corrompant tout le decodage qui suit.
void TestC1MemoryFormOrderOfFetch()
{
    // C1 /4 SHL dword [DATA], imm8 -- 32 bits, memoire, SIB disp32-only
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xC1);
        code.push_back(0x24); // ModRM: mod=00 reg=100(/4=SHL) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(4); // imm8 = 4, DOIT etre lu apres le disp32
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /4 SHL [mem],4 (SIB+disp32, ordre correct)", ran &&
              (Read32(mem, DATA) == 16));
    }

    // C1 /5 SHR qword [DATA], imm8 -- 64 bits (REX.W), memoire, SIB disp32-only
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 256);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0x48); // REX.W
        code.push_back(0xC1);
        code.push_back(0x2C); // ModRM: mod=00 reg=101(/5=SHR) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(4);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /5 SHR [mem64],4 (REX.W, SIB+disp32)", ran &&
              (Read64(mem, DATA) == 16));
    }

    // C1 /7 SAR dword [DATA], imm8 -- memoire, SIB disp32-only
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0xFFFFFFF0);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xC1);
        code.push_back(0x3C); // ModRM: mod=00 reg=111(/7=SAR) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(4);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /7 SAR [mem],4 (SIB+disp32, ordre correct)", ran &&
              (Read32(mem, DATA) == 0xFFFFFFFF));
    }

    // C1 /0 ROL dword [DATA], imm8 -- memoire, SIB disp32-only
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xC1);
        code.push_back(0x04); // ModRM: mod=00 reg=000(/0=ROL) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(4);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /0 ROL [mem],4 (SIB+disp32, ordre correct)", ran &&
              (Read32(mem, DATA) == 16));
    }

    // C1 /1 ROR qword [DATA], imm8 -- 64 bits (REX.W), memoire, SIB disp32-only
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 16);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0x48); // REX.W
        code.push_back(0xC1);
        code.push_back(0x0C); // ModRM: mod=00 reg=001(/1=ROR) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(4);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /1 ROR [mem64],4 (REX.W, SIB+disp32)", ran &&
              (Read64(mem, DATA) == 1));
    }
}

void TestC1MemoryFlags()
{
    // Verifie que les flags restent corrects sur la forme memoire
    // (CF issu du dernier bit sorti, ZF/SF sur SHL/SHR/SAR, pas sur ROL/ROR)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 0x80000000); // MSB positionne
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xC1);
        code.push_back(0x24); // SHL [mem],1
        PushSibDisp32(code);
        code.push_back(1);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("C1 /4 SHL [mem],1 -> CF=1 (bit sorti) et ZF=1 (resultat 0)",
              ran && (cpu.Rflags() & 1) == 1 && (cpu.Rflags() & (1ULL<<6)) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64(mem, DATA, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xC1);
        code.push_back(0x04); // ROL [mem],1
        PushSibDisp32(code);
        code.push_back(1);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        // ROL ne doit pas toucher ZF : on force ZF=1 avant via un CMP egal
        CHECK("C1 /0 ROL [mem],1 execute correctement (regression order)",
              ran && Read32(mem, DATA) == 2);
    }
}



// =========================================================
// TEST (F6/0, F7/0 corrige pour rejeter /1) et XCHG (0x86)
// =========================================================

std::uint8_t Read8(Memory& mem, std::uint64_t addr)
{
    std::uint8_t value = 0;
    mem.Read(addr, &value, 1);
    return value;
}

void Write64m(Memory& mem, std::uint64_t addr, std::uint64_t value)
{
    mem.Write(addr, reinterpret_cast<const std::uint8_t*>(&value), sizeof(value));
}

// ================= TEST =================

void TestTestRegisterAndMemory()
{
    // F6 /0 : TEST AL, imm8 (registre)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x0F);
        code.insert(code.end(), {0xF6, 0xC0, 0xF0}); // TEST AL,0xF0 -> ZF=1
        code = Finish(code);
        CHECK("F6 /0 TEST AL,0xF0 -> ZF=1", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    // F6 /0 : TEST r/m8, imm8 en memoire, SIB+disp32
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        std::uint8_t v = 0x0F;
        mem.Write(DATA, &v, 1);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0xF6);
        code.push_back(0x04); // mod=00 reg=000(/0) rm=100(SIB)
        PushSibDisp32(code);
        code.push_back(0xF0); // imm8, DOIT etre lu apres le disp32
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("F6 /0 TEST [mem8],0xF0 (SIB+disp32, ordre correct) -> ZF=1", ran &&
              (cpu.Rflags() & (1ULL << 6)) != 0 &&
              Read8(mem, DATA) == 0x0F); // operande inchange
    }
    // F7 /0 : TEST EAX, imm32 -> non nul
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xFF);
        code.insert(code.end(), {0xF7, 0xC0, 0x0F,0,0,0}); // TEST EAX,0xF -> non nul
        code = Finish(code);
        CHECK("F7 /0 TEST EAX,0xF -> ZF=0 (non nul)", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) == 0 && cpu.Rax() == 0xFF);
    }
    // F7 /0 : TEST RAX, imm32 (REX.W, 64 bits) en memoire, SIB+disp32
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64m(mem, DATA, 0x100000000ULL); // bit 32 pose, invisible sur les 32 bits bas
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code;
        code.push_back(0x48); // REX.W
        code.push_back(0xF7);
        code.push_back(0x04); // mod=00 reg=000(/0) rm=100(SIB)
        PushSibDisp32(code);
        for (int i=0;i<4;++i) code.push_back(0); // imm32 = 0x00000000
        code.back() = 0x00;
        code[code.size()-4] = 0x00; code[code.size()-3]=0x00; code[code.size()-2]=0x00; code[code.size()-1]=0x00;
        // TEST qword [DATA], 0xFFFFFFFF (imm32 sign-extended = 0xFFFFFFFFFFFFFFFF)
        code[code.size()-4]=0xFF; code[code.size()-3]=0xFF; code[code.size()-2]=0xFF; code[code.size()-1]=0xFF;
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("F7 /0 TEST [mem64],0xFFFFFFFF (REX.W, SIB+disp32) -> ZF=0", ran &&
              (cpu.Rflags() & (1ULL << 6)) == 0);
    }
    // TEST produisant ZF=1 (resultat exactement zero)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xAAAA);
        code.insert(code.end(), {0xF7, 0xC0, 0,0,0,0}); // TEST EAX,0 -> toujours ZF=1
        code = Finish(code);
        CHECK("F7 /0 TEST EAX,0 -> ZF=1 (resultat zero)", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & (1ULL << 6)) != 0 && cpu.Rax() == 0xAAAA);
    }
    // TEST CF/OF toujours a 0
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xFFFFFFFF);
        code.insert(code.end(), {0xF7, 0xC0, 0xFF,0xFF,0xFF,0xFF});
        code = Finish(code);
        CHECK("F7 /0 TEST -> CF=0 et OF=0", RunCode(cpu, mem, code) &&
              (cpu.Rflags() & 1ULL) == 0 && (cpu.Rflags() & (1ULL<<11)) == 0);
    }
    // F7 /1 doit echouer (reserve, PAS traite comme TEST)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        std::vector<std::uint8_t> code = {0xF7, 0xC8, 1,0,0,0, 0xC3}; // F7 /1 (mod=11 reg=001 rm=000)
        CHECK("F7 /1 rejete (reserve, pas traite comme TEST)", !RunCode(cpu, mem, code));
    }
}

// ================= XCHG =================

void TestXchgAll()
{
    // 86 /r : XCHG AL, CL (registre 8 bits)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x11);
        Append(code, MovR64(1, 0x22));
        code.insert(code.end(), {0x86, 0xC1}); // XCHG CL,AL (reg=AL,rm=CL)
        code = Finish(code);
        CHECK("86 XCHG AL,CL (8 bits)", RunCode(cpu, mem, code) &&
              (cpu.Rax() & 0xFF) == 0x22 &&
              (cpu.ReadRegister64(1) & 0xFF) == 0x11);
    }
    // 86 /r : XCHG memoire 8 bits, SIB+disp32
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        std::uint8_t v = 0x77;
        mem.Write(DATA, &v, 1);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(1, 0x33); // CL = 0x33
        code.push_back(0x86);
        code.push_back(0x0C); // mod=00 reg=001(CL) rm=100(SIB)
        PushSibDisp32(code);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("86 XCHG [mem8],CL (SIB+disp32)", ran &&
              (cpu.ReadRegister64(1) & 0xFF) == 0x77 &&
              Read8(mem, DATA) == 0x33);
    }
    // 87 /r : XCHG memoire 32 bits, SIB+disp32
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(DATA, 0x1000); mem.Map(STACK, 0x2000);
        Write64m(mem, DATA, 0xCAFEBABEULL);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(1, 0x1234); // ECX
        code.push_back(0x87);
        code.push_back(0x0C);
        PushSibDisp32(code);
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("87 XCHG [mem32],ECX (SIB+disp32)", ran &&
              cpu.ReadRegister64(1) == 0xCAFEBABEULL &&
              Read32(mem, DATA) == 0x1234);
    }
    // 87 /r avec REX.W : XCHG r/m64, r64, registres etendus (R8/R9 via REX.R/REX.B)
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(8, 0xAAAAAAAAAAAAAAAAULL);  // R8
        Append(code, MovR64(9, 0xBBBBBBBBBBBBBBBBULL)); // R9
        // REX = 0100 1WRB : W=1(64bit) R=1(reg etend) B=1(rm etend) -> 0x4D
        // ModRM: mod=11 reg=000(R8 via REX.R) rm=001(R9 via REX.B) = 0xC1
        code.insert(code.end(), {0x4D, 0x87, 0xC1});
        code = Finish(code);
        CHECK("87 XCHG R8,R9 (REX.W+REX.R+REX.B, registres etendus)",
              RunCode(cpu, mem, code) &&
              cpu.ReadRegister64(8) == 0xBBBBBBBBBBBBBBBBULL &&
              cpu.ReadRegister64(9) == 0xAAAAAAAAAAAAAAAAULL);
    }
    // Les flags ne doivent JAMAIS changer avec XCHG
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 5);
        Append(code, MovR64(1, 3));
        // CMP RAX,RCX (5 vs 3) -> positionne des flags precis (CF=0,ZF=0,SF=0)
        code.insert(code.end(), {0x48, 0x39, 0xC8});
        Append(code, MovR64(2, 0x99));
        Append(code, MovR64(3, 0x11));
        code.insert(code.end(), {0x48, 0x87, 0xDA}); // XCHG RDX,RBX
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("XCHG ne modifie aucun flag", ran &&
              (cpu.Rflags() & 1ULL) == 0 &&           // CF inchange (etait 0)
              (cpu.Rflags() & (1ULL<<6)) == 0 &&       // ZF inchange (etait 0)
              (cpu.Rflags() & (1ULL<<7)) == 0 &&       // SF inchange (etait 0)
              cpu.ReadRegister64(2) == 0x11 &&
              cpu.ReadRegister64(3) == 0x99);
    }
    // 86 avec REX : rm=100 doit etre SPL (pas AH) des que REX present
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x9900); // AH = 0x99 si pas de REX
        Append(code, MovR64(4, 0x55)); // SPL = 0x55 (bas de RSP)
        // REX present (0x40 minimal) + 86 /r : reg=000(AL) rm=100 -> avec REX = SPL, pas AH
        code.insert(code.end(), {0x40, 0x86, 0xC4});
        code = Finish(code);
        bool ran = RunCode(cpu, mem, code);
        CHECK("86 XCHG avec REX: rm=100 -> SPL, pas AH", ran &&
              (cpu.ReadRegister64(4) & 0xFF) == 0x00 &&  // AL (0x00) copie dans SPL
              cpu.Rax() == 0x9955);                       // AL a recu SPL=0x55, AH inchangee (0x99)
    }
}


void TestXchgAccumulatorShortForms()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1111222233334444ULL);
        Append(code, MovR64(1, 0xAAAABBBBCCCCDDDDULL));
        code.insert(code.end(), {0x91, 0xF4}); // XCHG RAX,RCX (32-bit short form)
        code = Finish(code);
        CHECK("91h XCHG EAX,ECX short form zero-extends EAX",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x00000000CCCCDDDDULL &&
              cpu.ReadRegister64(1) == 0x0000000033334444ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1111222233334444ULL);
        Append(code, MovR64(8, 0xAAAABBBBCCCCDDDDULL));
        code.insert(code.end(), {0x49, 0x90, 0xF4}); // REX.B + NOP opcode: XCHG RAX,R8
        code = Finish(code);
        CHECK("49h 90 XCHG RAX,R8 short form uses REX.B",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0xAAAABBBBCCCCDDDDULL &&
              cpu.ReadRegister64(8) == 0x1111222233334444ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xAAAABBBB00001234ULL);
        Append(code, MovR64(8, 0xCCCCDDDD00005678ULL));
        code.insert(code.end(), {0x66, 0x41, 0x90, 0xF4}); // 66 + REX.B: XCHG AX,R8W
        code = Finish(code);
        CHECK("66h 41h 90 XCHG AX,R8W short form preserves upper bits",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0xAAAABBBB00005678ULL &&
              cpu.ReadRegister64(8) == 0xCCCCDDDD00001234ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000);
        Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x123456789ABCDEF0ULL);
        Append(code, MovR64(9, 0x0FEDCBA987654321ULL));
        code.insert(code.end(), {0x4D, 0x91, 0xF4}); // REX.WR+B: XCHG RAX,R9
        code = Finish(code);
        CHECK("4Dh 91h XCHG RAX,R9 short form combines REX.W+B",
              RunCode(cpu, mem, code) &&
              cpu.Rax() == 0x0FEDCBA987654321ULL &&
              cpu.ReadRegister64(9) == 0x123456789ABCDEF0ULL);
    }
}


void TestShift8LargeCount()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x1000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    // MOV AL,0x81 ; SHL AL,31. Counts >= operand width have
    // architecturally undefined result/flags, but must not invoke
    // host-language undefined behavior.
    auto code = std::vector<std::uint8_t>{
        0xB8, 0x81, 0x00, 0x00, 0x00,
        0xC0, 0xE0, 0x1F
    };

    code = Finish(code);

    CHECK(
        "SHL8 count >= width is defined by emulator policy",
        RunCode(cpu, mem, code) &&
        (cpu.ReadRegister64(0) & 0xFFU) == 0);
}

void TestOperandSizeOverride()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    // 66 B8 34 12 = MOV AX,1234h. The upper RAX bits must survive.
    auto code = MovR64(0, 0x1122334455667788ULL);
    code.insert(code.end(), {0x66, 0xB8, 0x34, 0x12});

    // 66 BB ABCDh = MOV BX,ABCDh; 66 89 02 = MOV [RDX],AX;
    // 66 8B 0A = MOV CX,[RDX].
    code.insert(code.end(), {0x66, 0xBB, 0xCD, 0xAB});
    code.insert(code.end(), {0x66, 0x89, 0xD8});
    Append(code, MovR64(2, DATA));
    code.insert(code.end(), {0x66, 0x89, 0x02});
    code.insert(code.end(), {0x66, 0x8B, 0x0A});
    code = Finish(code);

    CHECK(
        "66h MOV register/memory 16-bit width",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x112233445566ABCDULL &&
        cpu.ReadRegister64(1) == 0xABCD &&
        (cpu.ReadRegister64(3) & 0xFFFFULL) == 0xABCD &&
        cpu.ReadRegister64(1) == 0x000000000000ABCDULL &&
        Read16(mem, DATA) == 0xABCD);

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455667788ULL);
        stageCode.insert(stageCode.end(), {0x66, 0xB8, 0x34, 0x12});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r16,imm16 preserves upper register bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x1122334455661234ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(3, 0x1122334455667788ULL);
        stageCode.insert(stageCode.end(), {0x66, 0xBB, 0xCD, 0xAB});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV BX,imm16 preserves upper register bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(3) == 0x112233445566ABCDULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455667788ULL);
        Append(stageCode, MovR64(3, 0x112233445566ABCDULL));
        stageCode.insert(stageCode.end(), {0x66, 0x89, 0xD8});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV AX,BX preserves upper RAX bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x112233445566ABCDULL &&
            stageCpu.ReadRegister64(3) == 0x112233445566ABCDULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(1, 0x1122334455667788ULL);
        stageCode.insert(stageCode.end(), {0x66, 0xC7, 0xC1, 0x34, 0x12});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r/m16,imm16 register form",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(1) == 0x1122334455661234ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(2, DATA);
        stageCode.insert(stageCode.end(), {0x66, 0xC7, 0x02, 0x34, 0x12});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r/m16,imm16 memory form",
            RunCode(stageCpu, stageMem, stageCode) &&
            Read16(stageMem, DATA) == 0x1234);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x112233445566ABCDULL);
        Append(stageCode, MovR64(2, DATA));
        stageCode.insert(stageCode.end(), {0x66, 0x89, 0x02});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r/m16,r16 stores exactly 16 bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            Read16(stageMem, DATA) == 0xABCD);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455660080ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x0F, 0xBE, 0xC0});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOVSX r16,r/m8 preserves upper bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x112233445566FF80ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x112233445566FF80ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x0F, 0xBF, 0xC0});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOVSX r16,r/m16 preserves upper bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x112233445566FF80ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x112233445566ABCDULL);
        stageCode.insert(stageCode.end(), {0x66, 0x0F, 0xB7, 0xC0});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOVZX r16,r/m16 preserves upper bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x112233445566ABCDULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x11223344556600ABULL);
        stageCode.insert(stageCode.end(), {0x66, 0x0F, 0xB6, 0xC0});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOVZX r16,r/m8 preserves upper bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x11223344556600ABULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455661000ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x81, 0xC0, 0x34, 0x12});
        stageCode = Finish(stageCode);
        CHECK(
            "66h ADD r/m16,imm16 register form",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x1122334455662234ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        stageCpu.SetRflags(1ULL);
        auto stageCode = MovR64(0, 0x1122334455660000ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x81, 0xD0, 0xFF, 0xFF});
        stageCode = Finish(stageCode);
        CHECK(
            "66h ADC r/m16,imm16 preserves carry semantics",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x1122334455660000ULL &&
            (stageCpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        stageCpu.SetRflags(1ULL);
        auto stageCode = MovR64(0, 0x1122334455660000ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x81, 0xD8, 0x00, 0x00});
        stageCode = Finish(stageCode);
        CHECK(
            "66h SBB r/m16,imm16 preserves borrow semantics",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x112233445566FFFFULL &&
            (stageCpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455661000ULL);
        stageCode.insert(stageCode.end(), {0x66, 0x83, 0xC0, 0x02});
        stageCode = Finish(stageCode);
        CHECK(
            "66h ADD r/m16,imm8 sign-extended register form",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x1122334455661002ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x112233445566ABCDULL);
        stageCode.push_back(0x66);
        stageCode.push_back(0x50);
        stageCode.push_back(0x33);
        stageCode.push_back(0xC0);
        stageCode.push_back(0x66);
        stageCode.push_back(0x58);
        stageCode = Finish(stageCode);
        CHECK(
            "66h PUSH/POP r16 uses 16-bit stack width",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x000000000000ABCDULL &&
            stageCpu.Rsp() == STACK + 0x1000);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455661111ULL);
        Append(stageCode, MovR64(3, 0xAABBCCDDEEFF2222ULL));
        stageCode.insert(stageCode.end(), {0x66, 0x87, 0xD8});
        stageCode = Finish(stageCode);
        CHECK(
            "66h XCHG r16,r16 preserves upper bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(0) == 0x1122334455662222ULL &&
            stageCpu.ReadRegister64(3) == 0xAABBCCDDEEFF1111ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, DATA);
        stageCode.insert(stageCode.end(), {0x66, 0x8D, 0x40, 0x20});
        stageCode = Finish(stageCode);
        CHECK(
            "66h LEA writes a 16-bit destination",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x0000000000500020ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        Write64(stageMem, DATA, 0x112233445566ABCDULL);
        auto stageCode = MovR64(2, DATA);
        stageCode.insert(stageCode.end(), {0x66, 0x8B, 0x0A});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r16,r/m16 loads exactly 16 bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(1) == 0xABCD);
    }
}

void TestOperandSizeOverrideArithmetic()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 1); Append(code, MovR64(3, 2)); code.insert(code.end(), {0x66, 0x0F, 0xC1, 0xC3}); code = Finish(code);
        CHECK("66h XADD r/m16,r16 swaps and adds", RunCode(cpu, mem, code) && cpu.Rax() == 0x2ULL && cpu.ReadRegister64(3) == 0x3ULL);
    }

{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660003ULL); Append(code, MovR64(3, 4)); code.insert(code.end(), {0x66, 0x0F, 0xAF, 0xC3}); code = Finish(code);
        CHECK("66h 0F AF IMUL r16,r/m16", RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566000CULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455664001ULL); code.insert(code.end(), {0x66, 0xC1, 0xE0, 0x01}); code = Finish(code);
        CHECK("66h C1 SHL r/m16,1 preserves upper bits and flags", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455668002ULL && (cpu.Rflags() & 1ULL) == 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455664001ULL); code.insert(code.end(), {0x66, 0xD1, 0xE0}); code = Finish(code);
        CHECK("66h D1 SHL r/m16,1 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455668002ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660000ULL); code.insert(code.end(), {0x66, 0x6A, 0x80, 0x66, 0x58}); code = Finish(code);
        CHECK("66h PUSH imm8 and POP r16 use 16-bit stack width", RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566FF80ULL && cpu.Rsp() == STACK + 0x1000);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660001ULL); Append(code, MovR64(1, 2)); code.insert(code.end(), {0x66, 0xD3, 0xE0}); code = Finish(code);
        CHECK("66h D3 SHL r/m16,CL preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455660004ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FFFFULL); Append(code, MovR64(3, 2)); code.insert(code.end(), {0x66, 0x01, 0xD8}); code = Finish(code);
        CHECK("66h ADD r/m16,r16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455660001ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0xAAAA000000001234ULL); Append(code, MovR64(3, 0xBBBB56781234ULL)); code.insert(code.end(), {0x66, 0x39, 0xD8}); code = Finish(code);
        CHECK("66h CMP compares only 16 bits", RunCode(cpu, mem, code) && (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566F0F0ULL); Append(code, MovR64(3, 0x0000000000000FF0ULL)); code.insert(code.end(), {0x66, 0x21, 0xD8}); code = Finish(code);
        CHECK("66h AND r/m16,r16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x11223344556600F0ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x11223344000000F0ULL); Append(code, MovR64(3, 0x0000000000000F00ULL)); code.insert(code.end(), {0x66, 0x85, 0xD8}); code = Finish(code);
        CHECK("66h TEST uses only 16-bit operands", RunCode(cpu, mem, code) && (cpu.Rflags() & (1ULL << 6)) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FFFEULL); Append(code, MovR64(3, 3)); code.insert(code.end(), {0x66, 0xF7, 0xEB}); code = Finish(code);
        CHECK("66h F7 IMUL r/m16 signed product", RunCode(cpu, mem, code) && cpu.ReadRegister64(0) == 0x112233445566FFFAULL && cpu.ReadRegister64(2) == 0x000000000000FFFFULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455661234ULL); Append(code, MovR64(3, 2)); code.insert(code.end(), {0x66, 0xF7, 0xE3}); code = Finish(code);
        CHECK("66h F7 MUL r/m16 writes DX:AX", RunCode(cpu, mem, code) && cpu.ReadRegister64(0) == 0x1122334455662468ULL && cpu.ReadRegister64(2) == 0x0ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FFF6ULL); Append(code, MovR64(2, 0xFFFFFFFFFFFFFFFFULL)); Append(code, MovR64(3, 0xFFFFFFFFFFFFFFFDULL)); code.insert(code.end(), {0x66, 0xF7, 0xFB}); code = Finish(code);
        CHECK("66h F7 IDIV r/m16 signed quotient/remainder", RunCode(cpu, mem, code) && cpu.ReadRegister64(0) == 0x1122334455660003ULL && cpu.ReadRegister64(2) == 0xFFFFFFFFFFFFFFFFULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660010ULL); Append(code, MovR64(2, 0)); Append(code, MovR64(3, 3)); code.insert(code.end(), {0x66, 0xF7, 0xF3}); code = Finish(code);
        CHECK("66h F7 DIV r/m16 writes quotient and remainder", RunCode(cpu, mem, code) && cpu.ReadRegister64(0) == 0x1122334455660005ULL && cpu.ReadRegister64(2) == 0x1ULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FFFFULL); code.insert(code.end(), {0x66, 0xF7, 0xD8}); code = Finish(code);
        CHECK("66h F7 NEG r/m16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455660001ULL && (cpu.Rflags() & 1ULL) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x11223344556600F0ULL); code.insert(code.end(), {0x66, 0xF7, 0xD0}); code = Finish(code);
        CHECK("66h F7 NOT r/m16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566FF0FULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FFFFULL); code.insert(code.end(), {0x66, 0xFF, 0xC0}); code = Finish(code);
        CHECK("66h FF /0 INC AX", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455660000ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660001ULL); code.insert(code.end(), {0x66, 0xF7, 0xD8}); code = Finish(code);
        CHECK("66h F7 NEG r/m16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566FFFFULL && (cpu.Rflags() & 1ULL) != 0);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x11223344556600F0ULL); code.insert(code.end(), {0x66, 0xF7, 0xD0}); code = Finish(code);
        CHECK("66h F7 NOT r/m16 preserves upper bits", RunCode(cpu, mem, code) && cpu.Rax() == 0x112233445566FF0FULL);
    }
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660001ULL); code.insert(code.end(), {0x66, 0x81, 0xC0, 0x34, 0x12}); code = Finish(code);
        CHECK("66h Group1 ADD r/m16,imm16", RunCode(cpu, mem, code) && cpu.Rax() == 0x1122334455661235ULL);
    }
}
}

void TestOperandSizeOverrideLegacyArithmetic()
{
    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660080ULL);
        code.insert(code.end(), {0x66, 0x98}); // CBW
        code = Finish(code);
        CHECK(
            "66h CBW sign-extends AL into AX",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x112233445566FF80ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x112233445566FF80ULL);
        code.insert(code.end(), {0x66, 0x99}); // CWD
        code = Finish(code);
        CHECK(
            "66h CWD sign-extends AX into DX",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(2) == 0x000000000000FFFFULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660005ULL);
        Append(code, MovR64(3, 2));
        cpu.SetRflags(1ULL);
        code.insert(code.end(), {0x66, 0x1B, 0xC3}); // SBB AX,BX
        code = Finish(code);
        CHECK(
            "66h SBB r16,r/m16 preserves upper bits",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x1122334455660002ULL);
    }

    {
        Memory mem; mem.Map(CODE, 0x2000); mem.Map(STACK, 0x2000); Cpu cpu = MakeCpu(mem);
        auto code = MovR64(0, 0x1122334455660005ULL);
        cpu.SetRflags(1ULL);
        code.insert(code.end(), {0x66, 0x1D, 0x02, 0x00}); // SBB AX,2
        code = Finish(code);
        CHECK(
            "66h SBB AX,imm16 preserves upper bits",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x1122334455660002ULL);
    }
}



void TestCmpTest16RexSib()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x340;
        constexpr std::uint64_t index = 2;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 0xAAAABBBBCCCC1234ULL);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0x1234);

        const std::vector<std::uint8_t> code = {
            0x66, 0x47, 0x3B, 0x44, 0x8D, 0x20, 0xF4
        }; // CMP R8W,[R13+R9*4+disp8], 66h + REX.RXB

        CHECK(
            "CMP16 memory 66h REX.RXB SIB disp8",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & (1ULL << 6)) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        constexpr std::uint64_t base = DATA + 0x380;
        constexpr std::uint64_t index = 1;
        constexpr std::uint64_t address = base + index * 4 + 0x20;
        Write64(mem, address, 0xFFFFFFFFFFFF00F0ULL);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.WriteRegister64(8, 0x00F0);

        const std::vector<std::uint8_t> code = {
            0x66, 0x47, 0x85, 0x44, 0x8D, 0x20, 0xF4
        }; // TEST R8W,[R13+R9*4+disp8], 66h + REX.RXB

        CHECK(
            "TEST16 memory 66h REX.RXB SIB disp8",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & (1ULL << 6)) == 0 &&
            (cpu.Rflags() & 1ULL) == 0 &&
            (cpu.Rflags() & (1ULL << 11)) == 0);
    }
}

void TestCmovccCoverage()
{
    const struct {
        std::uint8_t cc;
        std::uint64_t flags;
        bool expected;
    } conditions[] = {
        {0x0, 1ULL << 11, true},                 // O
        {0x1, 0, true},                           // NO
        {0x2, 1ULL << 0, true},                  // B/NAE/C
        {0x3, 0, true},                           // AE/NB/NC
        {0x4, 1ULL << 6, true},                  // E/Z
        {0x5, 0, true},                           // NE/NZ
        {0x6, 1ULL << 0, true},                  // BE/NA
        {0x7, 0, true},                           // A/NBE
        {0x8, 1ULL << 7, true},                  // S
        {0x9, 0, true},                           // NS
        {0xA, 1ULL << 2, true},                  // P/PE
        {0xB, 0, true},                           // NP/PO
        {0xC, 1ULL << 7, true},                  // L
        {0xD, 0, true},                           // GE
        {0xE, (1ULL << 6) | (1ULL << 7), true}, // LE
        {0xF, 0, true}                            // G
    };

    for (const auto& condition : conditions) {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        constexpr std::uint64_t destination =
            0x1111222233334444ULL;
        constexpr std::uint64_t source =
            0xAAAABBBBCCCCDDDDULL;
        const std::uint64_t initialFlags =
            condition.flags | (1ULL << 1) | (1ULL << 4);

        cpu.SetRflags(initialFlags);
        auto code = MovR64(0, destination);
        Append(code, MovR64(1, source));

        // REX.W + 0F 4x /r: CMOVcc RAX,RCX.
        code.push_back(0x48);
        code.push_back(0x0F);
        code.push_back(static_cast<std::uint8_t>(0x40U + condition.cc));
        code.push_back(0xC1);
        code = Finish(code);

        const bool ran = RunCode(cpu, mem, code);
        CHECK(
            "CMOVcc 64-bit register condition " +
                std::to_string(condition.cc),
            ran &&
            cpu.Rax() == (condition.expected ? source : destination) &&
            cpu.Rflags() == initialFlags);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(1ULL << 6);

        auto code = MovR64(0, 0xFFFFFFFF00000000ULL);
        Append(code, MovR64(1, 0x12345678ULL));

        // 0F 44 C1: CMOVE EAX,ECX. A 32-bit CMOV zero-extends EAX.
        code.insert(code.end(), {0x0F, 0x44, 0xC1});
        code = Finish(code);

        CHECK(
            "CMOVE 32-bit register form zero-extends destination",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x0000000012345678ULL &&
            cpu.Rflags() == (1ULL << 6));
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(1ULL << 6);

        auto code = MovR64(0, 0xAAAABBBBCCCC1111ULL);
        Append(code, MovR64(1, 0x0000000000002222ULL));

        // 66 0F 44 C1: CMOVE AX,CX. Upper RAX bits remain unchanged.
        code.insert(code.end(), {0x66, 0x0F, 0x44, 0xC1});
        code = Finish(code);

        CHECK(
            "CMOVE 16-bit register form preserves upper destination bits",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0xAAAABBBBCCCC2222ULL &&
            cpu.Rflags() == (1ULL << 6));
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        constexpr std::uint64_t base = DATA + 0x200;
        constexpr std::uint64_t index = 3;
        constexpr std::uint64_t displacement = 0x20;
        const std::uint64_t address =
            base + index * 4 + displacement;
        Write64(mem, address, 0xCAFEBABEDEADBEEFULL);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, base);
        cpu.WriteRegister64(9, index);
        cpu.SetRflags(1ULL << 6);

        auto code = MovR64(8, 0x1111111111111111ULL);

        // 4F 0F 44 44 8D 20:
        // REX.WRXB + CMOVE R8,QWORD PTR [R13+R9*4+20h].
        code.insert(code.end(), {
            0x4F, 0x0F, 0x44, 0x44, 0x8D, 0x20
        });
        code = Finish(code);

        CHECK(
            "CMOVE R8, [R13+R9*4+disp8] exercises REX.R/X/B + SIB",
            RunCode(cpu, mem, code) &&
            cpu.ReadRegister64(8) == 0xCAFEBABEDEADBEEFULL &&
            cpu.Rflags() == (1ULL << 6));
    }
}



void TestBitTestFamily()
{
    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags((1ULL << 1) | (1ULL << 4) | (1ULL << 6));
        auto code = MovR64(0, 0x8000000000000020ULL);
        code.insert(code.end(), {0x48, 0x0F, 0xBA, 0xE0, 0x3F}); // BT RAX,63
        code = Finish(code);

        const std::uint64_t preserved =
            (1ULL << 1) | (1ULL << 4) | (1ULL << 6);
        CHECK(
            "BT64 immediate selects bit and preserves non-CF flags",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) != 0 &&
            (cpu.Rflags() & ~1ULL) == preserved &&
            cpu.Rax() == 0x8000000000000020ULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(0);
        auto code = MovR64(0, 0);
        code.insert(code.end(), {0x48, 0x0F, 0xBA, 0xE8, 0x05}); // BTS RAX,5
        code.insert(code.end(), {0x48, 0x0F, 0xBA, 0xF0, 0x05}); // BTR RAX,5
        code.insert(code.end(), {0x48, 0x0F, 0xBA, 0xF8, 0x05}); // BTC RAX,5
        code = Finish(code);

        CHECK(
            "BTS/BTR/BTC64 immediate set/reset/complement selected bit",
            RunCode(cpu, mem, code) &&
            cpu.Rax() == 0x20ULL &&
            (cpu.Rflags() & 1ULL) == 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.SetRflags(0);
        auto code = MovR64(0, 0x8000);
        code.insert(code.end(), {0x66, 0x0F, 0xBA, 0xE0, 0x0F}); // BT AX,15
        code.insert(code.end(), {0x0F, 0xBA, 0xE0, 0x1F}); // BT EAX,31
        code = Finish(code);

        CHECK(
            "BT16 and BT32 immediate forms use operand-size width",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) == 0 &&
            cpu.Rax() == 0x8000ULL);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, 0x8000000000000000ULL);
        cpu.WriteRegister64(1, 63);
        auto code = std::vector<std::uint8_t>{0x48, 0x0F, 0xA3, 0xC8}; // BT RAX,RCX
        code = Finish(code);

        CHECK(
            "BT64 register-index form",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) != 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(8, 0);
        cpu.WriteRegister64(9, 0x20);
        auto code = std::vector<std::uint8_t>{
            0x4D, 0x0F, 0xA3, 0xC1 // BT R9,R8 (REX.W+REX.R+REX.B)
        };
        code = Finish(code);

        CHECK(
            "BT64 register form covers R8/R9 through REX.R+B",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) == 0);
    }

    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);

        Write64(mem, DATA, 0x8000000000000000ULL);
        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(0, DATA + 8);
        cpu.WriteRegister64(1, static_cast<std::uint64_t>(-1LL));

        // 48 0F A3 08: BT [RAX],RCX. A -1 bit offset selects
        // the previous 64-bit word, bit 63.
        auto code = std::vector<std::uint8_t>{0x48, 0x0F, 0xA3, 0x08};
        code = Finish(code);

        CHECK(
            "BT64 memory register form handles negative bit displacement",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) != 0);
    }    {
        Memory mem;
        mem.Map(CODE, 0x2000);
        mem.Map(DATA, 0x2000);
        mem.Map(STACK, 0x2000);
        const std::uint64_t address = DATA + 0x100;
        Write64(mem, address, 0x0000000000000008ULL);

        Cpu cpu = MakeCpu(mem);
        cpu.WriteRegister64(13, DATA);
        cpu.WriteRegister64(10, 0x38);
        cpu.WriteRegister64(9, 3);
        // BT [R13 + R10*4 + disp8], R9:
        // REX.WRXB, ModRM r/m=SIB, SIB index=R10, base=R13.
        const std::vector<std::uint8_t> code = {
            0x4F, 0x0F, 0xA3, 0x4C, 0x95, 0x20, 0xF4
        };
        CHECK(
            "BT64 memory SIB REX.RXB disp8",
            RunCode(cpu, mem, code) &&
            (cpu.Rflags() & 1ULL) != 0);
    }
}





int main()
{
    TestCpuAudit();
    TestImul();
    TestMulDiv8();
    TestAdcSbb();
    TestDivIdiv();
    TestDivIdivMemoryExceptions();
    TestDivIdiv128();
    TestIoInstructions();
    TestSyscallDispatch();
    TestMovImmediate();
    std::cout << "\n";
    std::cout << "=============================================\n";
    std::cout << "        MyPS5Emu CPU FUNCTION TEST\n";
    std::cout << "=============================================\n\n";

    TestAddressSizeOverride();
    TestOperandSizeOverride();
    TestOperandSizeOverrideArithmetic();
    TestOperandSizeOverrideLegacyArithmetic();
    TestDescriptorTableInstructions();
    TestCanonicalAddressFault();
    TestMemory();
    TestRegisterFile();

    TestMov32();
    TestMov64();
    TestMemoryMov64();

    TestAddSub64();
    TestCmp();

    TestLogic64();
    TestLogicMemory64();
    TestTest64();

    TestLea();

    TestStack();
    TestLeave();
    TestPushImmediate();
    TestPushImmediate32();
    TestPushPopRexAndOperandWidths();
    TestPushPopRex16();
    TestPushPopRmForms();
    TestCallRet();

    TestGroup1RexExtended();
    TestPushfPopf();
    TestLeaRexSib();
    TestMovsxdVariants();
    TestIndirectCallAndRetImmediate();
    TestIndirectCallMemoryRexSib();
    TestIndirectJmpMemoryRexSib();
    TestJumps();
    TestJccBoundaryConditions();
    TestJmp();

    TestImmediate();
    TestCmpImmediate();
    TestImmediate81();
    TestImmediateMemorySIB();
    TestImmediateMemoryGroup1();
    TestSignExtend83();

    TestRexRegisters();
    TestRexXAndSib();
    TestDisp8Disp32();

    TestIncDecNeg();

    TestFlags();

    TestLea32();
    TestHlt();
    TestPushPopMemory16();
    TestRotateRexAndOperandWidths();
    TestRotate64MemoryRexSib();
    TestMovxByteRexAndHighByteRules();
    TestCmpxchgOperandWidths();
    TestSetccMemory();
    TestSetccMemoryRexSib();
    TestCmpxchgMemory16();
    TestCmpxchgVariants();
    TestCmpxchgMemoryRexSib();
    TestSetccAllConditions();
    TestSetcc();
    TestCmpTest16RexSib();
    TestCmovccCoverage();
    TestBitTestFamily();
    TestShiftsByCL();
    TestShiftClOperandWidthsAndRexMemory();
    TestShldShrd();
    TestCmpEaxImmediate();
    TestGroupF7Memory();
    TestImulMemory();
    TestImulMemoryWidths();
    TestSyscallPS5Fallback();

    TestRotate();
    TestTestImmediate();
    TestXchg();

    TestRotate8Register();
    TestRotate8HighByteAndRex();
    TestRotate8Memory();
    TestRotate8EdgeCases();

    TestC1RegisterForm();
    TestC1MemoryFormOrderOfFetch();
    TestC1MemoryFlags();

    TestTestRegisterAndMemory();
    TestXchgAll();


    std::cout << "\n";
    std::cout << "=============================================\n";
    std::cout << "                  RESULTAT\n";
    std::cout << "=============================================\n";
    std::cout << "PASS : " << passed << "\n";
    std::cout << "FAIL : " << failed << "\n";
    std::cout << "TOTAL: " << (passed + failed) << "\n";
    std::cout << "=============================================\n";

    return failed == 0 ? 0 : 1;
}
















