#include "cpu/Cpu.hpp"
#include "cpu/x86/Paging.hpp"
#include "cpu/x86/Gdt.hpp"
#include "cpu/x86/Idt.hpp"
#include "cpu/x86/Tss64.hpp"
#include "memory/Memory.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {
void Q(Memory& m, std::uint64_t a, std::uint64_t v) {
    m.Write(a, reinterpret_cast<const std::uint8_t*>(&v), sizeof(v));
}
bool Fail(const char* s) { std::cerr << s << '\n'; return false; }
}

int main() {
    Memory mem;
    if (!mem.Map(0x1000, 0x5000, MemoryPermission::Read | MemoryPermission::Write) ||
        !mem.Map(0x8000, 0x1000, MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute)) {
        return 1;
    }

    // Linear 0x400000 -> physical 0x8000 through a 4-level page walk.
    Q(mem, 0x1000, 0x2000 | 0x7);
    Q(mem, 0x2000, 0x3000 | 0x7);
    Q(mem, 0x3010, 0x4000 | 0x7); // PD index 2.
    Q(mem, 0x4000, 0x8000 | 0x7);

    const std::uint8_t hlt = 0xF4;
    mem.Write(0x8000, &hlt, 1);

    Paging paging(mem);
    Cpu cpu;
    cpu.ConnectMemory(&mem);
    cpu.SetPaging(&paging);
    cpu.SetCr3(0x1000);
    cpu.SetCr4(1ULL << 5);   // PAE.
    cpu.SetCr0(1ULL << 31);  // PG.
    cpu.SetCodeSegment(0x8);
    cpu.SetStackSegment(0x10);
    cpu.SetStackPointer(0x8FF0);
    cpu.SetRflags(0x246);
    cpu.SetInstructionPointer(0x400000);

    if (cpu.Run() != 0) {
        return Fail("CPU failed to fetch through paging") ? 0 : 1;
    }

    // An unmapped instruction fetch must become #PF and update CR2.
    // The regression also verifies that a configured IDT/GDT/TSS path takes
    // precedence over the legacy exception callback.
    Gdt64 gdt;
    GdtCodeSegment64 kernel_code{};
    kernel_code.present = true;
    kernel_code.long_mode = true;
    kernel_code.dpl = 0;
    if (!gdt.SetCodeSegment(1, kernel_code)) {
        return Fail("failed to install paging exception code segment") ? 0 : 1;
    }

    Idt idt;
    IdtGate64 gate{};
    gate.present = true;
    gate.selector = 1U << 3;
    gate.offset = 0x8000;
    if (!idt.SetGate(14, gate)) {
        return Fail("failed to install paging exception gate") ? 0 : 1;
    }

    Tss64 tss;
    tss.SetRsp0(0x9000);
    cpu.SetExceptionArchitecture(&idt, &gdt, &tss);

    bool legacy_callback_called = false;
    cpu.SetExceptionHandler([&](Cpu&, const CpuException&) {
        legacy_callback_called = true;
        return true;
    });

    cpu.SetCodeSegment(0x1B);
    cpu.SetStackSegment(0x23);
    cpu.SetStackPointer(0x8FF0);
    cpu.SetInstructionPointer(0x900000);
    if (cpu.Run() == 0 || legacy_callback_called ||
        cpu.Cr2() != 0x900000 ||
        cpu.LastException().page_fault_error != (1U << 4) ||
        cpu.CodeSegment() != 0x08 ||
        cpu.StackSegment() != 0 ||
        cpu.InstructionPointer() != gate.offset ||
        cpu.Rsp() != 0x8FD0) {
        return Fail("CPU did not route paging #PF through hardware exception delivery") ? 0 : 1;
    }

    std::cout << "x86 CPU paging integration test: PASS\n";
    return 0;
}
