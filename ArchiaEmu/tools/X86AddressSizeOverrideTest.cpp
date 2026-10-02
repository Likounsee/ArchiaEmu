#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

namespace {

bool Write32(Memory& memory, std::uint64_t address, std::uint32_t value)
{
    return memory.Write(
        address,
        reinterpret_cast<const std::uint8_t*>(&value),
        sizeof(value));
}

bool Run(Memory& memory, Cpu& cpu)
{
    cpu.ConnectMemory(&memory);
    cpu.SetInstructionPointer(0x1000);
    return cpu.Run() == 0;
}

} // namespace

int main()
{
    Memory memory;
    if (!memory.Map(
            0x1000,
            0x1000,
            MemoryPermission::Read |
            MemoryPermission::Write |
            MemoryPermission::Execute) ||
        !memory.Map(
            0x2000,
            0x1000,
            MemoryPermission::Read |
            MemoryPermission::Write)) {
        return 1;
    }

    // 67h switches a 64-bit code segment to 32-bit effective addresses.
    // The high half of RAX must be ignored when it is used as the base.
    const std::uint8_t baseCode[] = {
        0x67, 0x8B, 0x00, // MOV EAX, [EAX]
        0xF4
    };
    if (!memory.Write(0x1000, baseCode, sizeof(baseCode)) ||
        !Write32(memory, 0x2000, 0x11223344U)) {
        return 2;
    }

    Cpu baseCpu;
    baseCpu.WriteRegister64(0, 0x0000000100002000ULL);
    if (!Run(memory, baseCpu) || baseCpu.Rax() != 0x0000000011223344ULL) {
        std::cerr << "32-bit address-size override ignored high base bits\n";
        return 3;
    }

    // The SIB form must also perform all arithmetic at 32-bit width and
    // truncate the final effective address to 32 bits.
    const std::uint8_t sibCode[] = {
        0x67, 0x8B, 0x44, 0x88, 0x04, // MOV EAX, [EAX + ECX*4 + 4]
        0xF4
    };
    if (!memory.Write(0x1000, sibCode, sizeof(sibCode)) ||
        !Write32(memory, 0x2010, 0x55667788U)) {
        return 4;
    }

    Cpu sibCpu;
    sibCpu.WriteRegister64(0, 0x0000000100002000ULL);
    sibCpu.WriteRegister64(1, 0x0000000100000003ULL);
    if (!Run(memory, sibCpu) || sibCpu.Rax() != 0x0000000055667788ULL) {
        std::cerr << "32-bit SIB address-size override failed\n";
        return 5;
    }

    // In long mode, 67h also changes ModRM r/m=101 from RIP-relative
    // addressing to an absolute 32-bit displacement.
    const std::uint8_t dispOnlyCode[] = {
        0x67, 0x8B, 0x05, 0x10, 0x20, 0x00, 0x00, // MOV EAX, [disp32]
        0xF4
    };
    if (!memory.Write(0x1000, dispOnlyCode, sizeof(dispOnlyCode)) ||
        !Write32(memory, 0x2010, 0xA1B2C3D4U)) {
        return 6;
    }

    Cpu dispOnlyCpu;
    if (!Run(memory, dispOnlyCpu) || dispOnlyCpu.Rax() != 0x00000000A1B2C3D4ULL) {
        std::cerr << "32-bit displacement-only address-size override failed\n";
        return 7;
    }

    // SIB base=5, index=4 is the no-base/no-index encoding. Under 67h it
    // must likewise use the zero-extended 32-bit displacement as the address.
    const std::uint8_t sibNoBaseCode[] = {
        0x67, 0x8B, 0x04, 0x25, 0x10, 0x20, 0x00, 0x00, // MOV EAX, [disp32]
        0xF4
    };
    if (!memory.Write(0x1000, sibNoBaseCode, sizeof(sibNoBaseCode)) ||
        !Write32(memory, 0x2010, 0xCAFEBABEU)) {
        return 8;
    }

    Cpu sibNoBaseCpu;
    if (!Run(memory, sibNoBaseCpu) || sibNoBaseCpu.Rax() != 0x00000000CAFEBABEULL) {
        std::cerr << "32-bit SIB no-base address-size override failed\n";
        return 9;
    }

    // 67h must compose correctly with REX.X/B: the extended base/index
    // registers are still read at 32-bit width and the destination extension
    // from REX.R must remain independent of address calculation.
    const std::uint8_t rexSibCode[] = {
        0x67, 0x47, 0x8B, 0x44, 0x88, 0x04, // MOV R8D, [R8D + R9D*4 + 4]
        0xF4
    };
    if (!memory.Write(0x1000, rexSibCode, sizeof(rexSibCode)) ||
        !Write32(memory, 0x2010, 0x0BADF00DU)) {
        return 10;
    }

    Cpu rexSibCpu;
    rexSibCpu.WriteRegister64(8, 0x0000000100002000ULL);
    rexSibCpu.WriteRegister64(9, 0x0000000100000003ULL);
    if (!Run(memory, rexSibCpu) || rexSibCpu.ReadRegister64(8) != 0x000000000BADF00DULL) {
        std::cerr << "REX + 32-bit SIB address-size override failed\n";
        return 11;
    }

    // REX.W changes only operand width: 67h must still constrain the
    // effective address to 32 bits while MOV loads the full 64-bit value.
    const std::uint8_t rexWCode[] = {
        0x67, 0x48, 0x8B, 0x00, // MOV RAX, [EAX]
        0xF4
    };
    if (!memory.Write(0x1000, rexWCode, sizeof(rexWCode))) {
        return 12;
    }
    const std::uint64_t wideValue = 0x8877665544332211ULL;
    if (!memory.Write(0x2000, reinterpret_cast<const std::uint8_t*>(&wideValue), sizeof(wideValue))) {
        return 13;
    }

    Cpu rexWCpu;
    rexWCpu.WriteRegister64(0, 0x0000000100002000ULL);
    if (!Run(memory, rexWCpu) || rexWCpu.Rax() != wideValue) {
        std::cerr << "REX.W + 32-bit address-size override failed\n";
        return 14;
    }

    // LEA must use the same 32-bit effective-address rules, while its
    // 64-bit destination receives the zero-extended effective address.
    const std::uint8_t leaCode[] = {
        0x67, 0x48, 0x8D, 0x44, 0x88, 0x04, // LEA RAX, [EAX + ECX*4 + 4]
        0xF4
    };
    if (!memory.Write(0x1000, leaCode, sizeof(leaCode))) {
        return 15;
    }

    Cpu leaCpu;
    leaCpu.WriteRegister64(0, 0x0000000100002000ULL);
    leaCpu.WriteRegister64(1, 0x0000000100000003ULL);
    if (!Run(memory, leaCpu) || leaCpu.Rax() != 0x0000000000002010ULL) {
        std::cerr << "LEA + 32-bit address-size override failed\n";
        return 16;
    }

    std::cout << "x86 address-size override test: PASS\n";
    return 0;
}
