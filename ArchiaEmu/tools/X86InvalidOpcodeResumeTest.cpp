#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

int main()
{
    Memory memory;
    if (!memory.Map(0x1000, 0x1000,
                    MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute)) {
        return 1;
    }

    const std::uint8_t code[] = {0xD6, 0xF4};
    if (!memory.Write(0x1000, code, sizeof(code))) {
        return 1;
    }

    Cpu cpu;
    cpu.ConnectMemory(&memory);
    cpu.SetInstructionPointer(0x1000);

    bool called = false;
    cpu.SetExceptionHandler([&](Cpu& handlerCpu, const CpuException& exception) {
        called = exception.vector == CpuExceptionVector::InvalidOpcode;
        handlerCpu.SetInstructionPointer(0x1001);
        return true;
    });

    if (cpu.Run() != 0 || !called) {
        std::cerr << "invalid-opcode exception did not resume execution\n";
        return 1;
    }

    std::cout << "x86 invalid-opcode exception resume test: PASS\n";
    return 0;
}
