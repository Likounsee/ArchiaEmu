#include "cpu/x86/Paging.hpp"
#include "memory/Memory.hpp"

#include <array>
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
    if (!mem.Map(0x1000, 0x5000, MemoryPermission::Read | MemoryPermission::Write)) {
        return 1;
    }

    // PML4=0x1000, PDPT=0x2000, PD=0x3000, PT=0x4000.
    Q(mem, 0x1000, 0x2000 | 0x7);
    Q(mem, 0x2008, 0x2000 | 0x7);
    Q(mem, 0x2000, 0x3000 | 0x7);
    Q(mem, 0x3000, 0x4000 | 0x7);
    Q(mem, 0x4000, 0x8000 | 0x7);
    if (!mem.Map(0x8000, 0x1000, MemoryPermission::Read | MemoryPermission::Write | MemoryPermission::Execute)) {
        return 1;
    }

    Paging paging(mem);
    paging.SetCr3(0x1000);
    paging.SetCr4(1ULL << 5);

    auto r = paging.Translate(0x123, false, false, false);
    if (!r.ok || r.physical_address != 0x8123) return Fail("4-level 4KiB translation failed") ? 0 : 1;

    r = paging.Translate(0x123, true, true, false);
    if (!r.ok) return Fail("user writable translation failed") ? 0 : 1;

    // Clear PT writable/user bits and verify protection.
    Q(mem, 0x4000, 0x8000 | 0x1);
    r = paging.Translate(0x123, true, true, false);
    if (r.ok || r.fault != PagingFault::User || (r.page_fault_error & 0x7) != 0x7) {
        return Fail("write/user page fault flags incorrect") ? 0 : 1;
    }

    // Restore mapping and make it NX. Instruction fetch must fault when NXE is enabled.
    Q(mem, 0x4000, 0x8000 | 0x7 | (1ULL << 63));
    paging.SetEfer(1ULL << 11);
    r = paging.Translate(0x123, false, false, true);
    if (r.ok || r.fault != PagingFault::Instruction || (r.page_fault_error & (1U << 4)) == 0) {
        return Fail("NX instruction fault incorrect") ? 0 : 1;
    }

    // 2 MiB large page.
    Q(mem, 0x3000, 0x800000 | 0x87);
    r = paging.Translate(0x12345, false, false, false);
    if (!r.ok || r.physical_address != 0x812345) return Fail("2MiB translation failed") ? 0 : 1;

    // 1 GiB large page at the PDPT level.
    Q(mem, 0x2000, 0x40000000ULL | 0x87);
    r = paging.Translate(0x12345678, false, false, false);
    if (!r.ok || r.physical_address != 0x52345678ULL) return Fail("1GiB translation failed") ? 0 : 1;

    std::cout << "x86 paging test: PASS\n";
    return 0;
}
