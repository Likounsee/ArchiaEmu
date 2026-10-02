#include "cpu/Cpu.hpp"
#include "cpu/x86/ExceptionReturn64.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {

void WriteQword(Memory& memory, std::uint64_t address, std::uint64_t value)
{
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
    if (!memory.Write(address, bytes.data(), bytes.size())) {
        std::cerr << "failed to write IRETQ frame\n";
        std::exit(1);
    }
}

Gdt64 MakeGdt()
{
    Gdt64 gdt;

    GdtCodeSegment64 kernel{};
    kernel.present = true;
    kernel.long_mode = true;
    kernel.dpl = 0;
    gdt.SetCodeSegment(5, kernel);

    GdtDataSegment64 kernelStack{};
    kernelStack.present = true;
    kernelStack.writable = true;
    kernelStack.dpl = 0;
    gdt.SetDataSegment(2, kernelStack);

    GdtCodeSegment64 user{};
    user.present = true;
    user.long_mode = true;
    user.dpl = 3;
    gdt.SetCodeSegment(6, user);

    GdtDataSegment64 userStack{};
    userStack.present = true;
    userStack.writable = true;
    userStack.dpl = 3;
    gdt.SetDataSegment(7, userStack);

    return gdt;
}

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    Memory memory;
    if (!memory.Map(0x1000, 0x1000, MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute) ||
        !memory.Map(0x2000, 0x1000, MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute) ||
        !memory.Map(0x7000, 0x1000, MemoryPermission::Read | MemoryPermission::Write) ||
        !memory.Map(0x3000, 0x1000, MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute)) {
        return Fail("Failed to map IRETQ test memory") ? 0 : 1;
    }

    const std::uint8_t iretq[] = {0x48, 0xCF};
    const std::uint8_t hlt[] = {0xF4};
    if (!memory.Write(0x1000, iretq, sizeof(iretq)) ||
        !memory.Write(0x2000, hlt, sizeof(hlt))) {
        return Fail("Failed to write IRETQ program") ? 0 : 1;
    }

    Gdt64 gdt = MakeGdt();
    const std::uint8_t bareIret[] = {0xCF};
    if (!memory.Write(0x3000, bareIret, sizeof(bareIret))) return 1;
    Cpu bareIretCpu;
    bareIretCpu.ConnectMemory(&memory);
    bareIretCpu.SetInstructionPointer(0x3000);
    bool bareIretHandlerCalled = false;
    bareIretCpu.SetExceptionReturnHandler([&](Cpu&) { bareIretHandlerCalled = true; return true; });
    if (bareIretCpu.Run() == 0 || bareIretHandlerCalled) {
        return Fail("Bare CF was incorrectly dispatched as IRETQ") ? 0 : 1;
    }


    Cpu cpu;
    cpu.ConnectMemory(&memory);
    cpu.SetInstructionPointer(0x1000);
    cpu.SetCodeSegment(0x28);
    cpu.SetStackSegment(0x10);
    cpu.SetStackPointer(0x7000);
    cpu.SetRflags(0x202);

    WriteQword(memory, 0x7000, 0x2000);
    WriteQword(memory, 0x7008, 0x28);
    WriteQword(memory, 0x7010, 0x202);
    WriteQword(memory, 0x7018, 0x7600);
    WriteQword(memory, 0x7020, 0x10);

    bool handlerCalled = false;
    cpu.SetExceptionReturnHandler([&](Cpu& handlerCpu) {
        handlerCalled = true;
        const auto result =
            ExceptionReturn64::Read(handlerCpu, memory, gdt);
        if (result.status != ExceptionReturnStatus::Returned) {
            return false;
        }
        return ExceptionReturn64::Apply(handlerCpu, result).status ==
               ExceptionReturnStatus::Returned;
    });

    if (cpu.Run() != 0 || !handlerCalled ||
        cpu.InstructionPointer() != 0x2001 ||
        cpu.CodeSegment() != 0x28 ||
        cpu.Rsp() != 0x7600 ||
        cpu.StackSegment() != 0x10 ||
        cpu.Rflags() != 0x202) {
        return Fail("IRETQ instruction execution failed") ? 0 : 1;
    }

    Cpu noHandler;
    noHandler.ConnectMemory(&memory);
    noHandler.SetInstructionPointer(0x1000);
    noHandler.SetStackPointer(0x7000);
    if (noHandler.Run() == 0 ||
        noHandler.InstructionPointer() != 0x1002) {
        return Fail("IRETQ without a handler was not rejected") ? 0 : 1;
    }

    WriteQword(memory, 0x7000, 0x2000);
    WriteQword(memory, 0x7008, 0x33);
    WriteQword(memory, 0x7010, 0x202);
    WriteQword(memory, 0x7018, 0x7800);
    WriteQword(memory, 0x7020, 0x3B);

    Cpu privilegeReturn;
    privilegeReturn.ConnectMemory(&memory);
    privilegeReturn.SetInstructionPointer(0x1000);
    privilegeReturn.SetCodeSegment(0x28);
    privilegeReturn.SetStackSegment(0x10);
    privilegeReturn.SetStackPointer(0x7000);
    privilegeReturn.SetRflags(0x202);

    bool privilegeHandlerCalled = false;
    privilegeReturn.SetExceptionReturnHandler([&](Cpu& handlerCpu) {
        privilegeHandlerCalled = true;
        const auto result =
            ExceptionReturn64::Read(handlerCpu, memory, gdt);
        if (result.status != ExceptionReturnStatus::Returned) {
            return false;
        }
        return ExceptionReturn64::Apply(handlerCpu, result).status ==
               ExceptionReturnStatus::Returned;
    });

    if (privilegeReturn.Run() != 0 || !privilegeHandlerCalled ||
        privilegeReturn.InstructionPointer() != 0x2001 ||
        privilegeReturn.CodeSegment() != 0x33 ||
        privilegeReturn.Rsp() != 0x7800 ||
        privilegeReturn.StackSegment() != 0x3B ||
        privilegeReturn.Rflags() != 0x202) {
        return Fail("IRETQ privilege return failed") ? 0 : 1;
    }

    WriteQword(memory, 0x7000, 0x0001000000000000ULL);

    Cpu invalidFrame;
    invalidFrame.ConnectMemory(&memory);
    invalidFrame.SetInstructionPointer(0x1000);
    invalidFrame.SetCodeSegment(0x28);
    invalidFrame.SetStackSegment(0x10);
    invalidFrame.SetStackPointer(0x7000);
    invalidFrame.SetRflags(0x202);

    invalidFrame.SetExceptionReturnHandler([&](Cpu& handlerCpu) {
        const auto result =
            ExceptionReturn64::Read(handlerCpu, memory, gdt);
        return ExceptionReturn64::Apply(handlerCpu, result).status ==
               ExceptionReturnStatus::Returned;
    });

    const auto beforeRsp = invalidFrame.Rsp();
    const auto beforeCs = invalidFrame.CodeSegment();
    const auto beforeFlags = invalidFrame.Rflags();
    if (invalidFrame.Run() == 0 ||
        invalidFrame.Rsp() != beforeRsp ||
        invalidFrame.CodeSegment() != beforeCs ||
        invalidFrame.Rflags() != beforeFlags) {
        return Fail("Invalid IRETQ frame modified CPU state") ? 0 : 1;
    }

    std::cout << "x86-64 IRETQ CPU integration test: PASS\n";
    return 0;
}
