#include "cpu/Cpu.hpp"
#include "cpu/x86/Paging.hpp"
#include "memory/Memory.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

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
    cpu.SetCodeSegment(0);
    cpu.SetInstructionPointer(0x400000);

    if (cpu.Run() != 0) {
        return Fail("CPU failed to fetch through paging") ? 0 : 1;
    }

    // An unmapped instruction fetch must become #PF and update CR2.
    cpu.SetInstructionPointer(0x900000);
    bool handled = false;
    cpu.SetExceptionHandler([&](Cpu&, const CpuException& e) {
        handled = e.vector == CpuExceptionVector::PageFault &&
                  e.page_fault_address == 0x900000;
        return true;
    });
    if (cpu.Run() == 0 || !handled || cpu.Cr2() != 0x900000 ||
        cpu.LastException().page_fault_error != 0) {
        std::cerr << "handled=" << handled
                  << " cr2=0x" << std::hex << cpu.Cr2()
                  << " vector=" << static_cast<int>(cpu.LastException().vector)
                  << " error=" << std::dec << cpu.LastException().page_fault_error << "\n";
        return Fail("CPU did not generate the expected paging #PF") ? 0 : 1;
    }

    std::cout << "x86 CPU paging integration test: PASS\n";
    return 0;
}
