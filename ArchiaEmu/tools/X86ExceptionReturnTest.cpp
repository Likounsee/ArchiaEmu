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
    memory.Write(address, bytes.data(), bytes.size());
}

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

Gdt64 MakeGdt()
{
    Gdt64 gdt;
    GdtCodeSegment64 kernel{};
    kernel.present = true;
    kernel.long_mode = true;
    kernel.dpl = 0;
    gdt.SetCodeSegment(5, kernel);

    GdtCodeSegment64 user{};
    user.present = true;
    user.long_mode = true;
    user.dpl = 3;
    gdt.SetCodeSegment(6, user);
    return gdt;
}

} // namespace

int main()
{
    Gdt64 gdt = MakeGdt();

    Memory memory;
    if (!memory.Map(0x7000, 0x1000,
                    MemoryPermission::Read | MemoryPermission::Write)) {
        return Fail("Failed to map return frame") ? 0 : 1;
    }

    Cpu cpu;
    cpu.SetInstructionPointer(0xAAAA);
    cpu.SetCodeSegment(0x28);
    cpu.SetStackPointer(0x7000);
    cpu.SetStackSegment(0x10);
    cpu.SetRflags(0x202);

    WriteQword(memory, 0x7000, 0x0000000000401234ULL);
    WriteQword(memory, 0x7008, 0x28);
    WriteQword(memory, 0x7010, 0x202);

    auto result = ExceptionReturn64::Read(cpu, memory, gdt);
    if (result.status != ExceptionReturnStatus::Returned ||
        result.rip != 0x401234 ||
        result.cs != 0x28 ||
        result.rflags != 0x202 ||
        result.rsp != 0x7018 ||
        result.ss != 0x10) {
        return Fail("Same-CPL IRETQ frame decode failed") ? 0 : 1;
    }

    const auto applied = ExceptionReturn64::Apply(cpu, result);
    if (applied.status != ExceptionReturnStatus::Returned ||
        cpu.InstructionPointer() != 0x401234 ||
        cpu.CodeSegment() != 0x28 ||
        cpu.Rsp() != 0x7018 ||
        cpu.StackSegment() != 0x10 ||
        cpu.Rflags() != 0x202) {
        return Fail("Same-CPL IRETQ apply failed") ? 0 : 1;
    }

    Cpu userCpu;
    userCpu.SetInstructionPointer(0xDEAD);
    userCpu.SetCodeSegment(0x28);
    userCpu.SetStackPointer(0x7100);
    userCpu.SetStackSegment(0x10);
    userCpu.SetRflags(0x202);

    WriteQword(memory, 0x7100, 0x0000000000505678ULL);
    WriteQword(memory, 0x7108, 0x33);
    WriteQword(memory, 0x7110, 0x202);
    WriteQword(memory, 0x7118, 0x0000000000800000ULL);
    WriteQword(memory, 0x7120, 0x3B);

    result = ExceptionReturn64::Read(userCpu, memory, gdt);
    if (result.status != ExceptionReturnStatus::Returned ||
        result.rip != 0x505678 ||
        result.cs != 0x33 ||
        result.rsp != 0x800000 ||
        result.ss != 0x3B) {
        return Fail("Privilege-return IRETQ frame decode failed") ? 0 : 1;
    }

    const auto beforeRip = userCpu.InstructionPointer();
    WriteQword(memory, 0x7100, 0x0001000000000000ULL);
    result = ExceptionReturn64::Read(userCpu, memory, gdt);
    if (result.status != ExceptionReturnStatus::InvalidInstructionPointer ||
        userCpu.InstructionPointer() != beforeRip) {
        return Fail("Non-canonical RIP was accepted") ? 0 : 1;
    }

    WriteQword(memory, 0x7100, 0x505678);
    WriteQword(memory, 0x7108, 0x40);
    result = ExceptionReturn64::Read(userCpu, memory, gdt);
    if (result.status != ExceptionReturnStatus::InvalidCodeSegment) {
        return Fail("Invalid CS was accepted") ? 0 : 1;
    }

    WriteQword(memory, 0x7108, 0x33);
    WriteQword(memory, 0x7110, 0x200);
    result = ExceptionReturn64::Read(userCpu, memory, gdt);
    if (result.status != ExceptionReturnStatus::InvalidRflags) {
        return Fail("Invalid RFLAGS was accepted") ? 0 : 1;
    }

    Memory readOnly;
    if (!readOnly.Map(0x7000, 0x1000, MemoryPermission::Write)) {
        return Fail("Failed to map read-only frame") ? 0 : 1;
    }
    WriteQword(readOnly, 0x7000, 0x401234);
    result = ExceptionReturn64::Read(cpu, readOnly, gdt);
    if (result.status != ExceptionReturnStatus::PermissionDenied) {
        return Fail("Read-only frame handling failed") ? 0 : 1;
    }

    Cpu unchanged;
    unchanged.SetInstructionPointer(0x1111);
    unchanged.SetCodeSegment(0x28);
    unchanged.SetStackPointer(0x7100);
    unchanged.SetStackSegment(0x10);
    unchanged.SetRflags(0x202);
    const auto failedApply = ExceptionReturn64::Apply(unchanged, result);
    if (failedApply.status != ExceptionReturnStatus::InvalidFrame ||
        unchanged.InstructionPointer() != 0x1111 ||
        unchanged.CodeSegment() != 0x28 ||
        unchanged.Rsp() != 0x7100 ||
        unchanged.StackSegment() != 0x10 ||
        unchanged.Rflags() != 0x202) {
        return Fail("Failed IRETQ modified CPU state") ? 0 : 1;
    }

    std::cout << "x86-64 exception return test: PASS\n";
    return 0;
}
