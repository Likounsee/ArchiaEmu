#pragma once

#include <cstdint>

namespace myps5emu {

class Memory;

enum class PagingFault : std::uint8_t {
    None,
    NotPresent,
    Write,
    User,
    Reserved,
    Instruction,
    Malformed
};

struct PagingResult {
    bool ok = false;
    std::uint64_t physical_address = 0;
    PagingFault fault = PagingFault::None;
    std::uint32_t page_fault_error = 0;
};

class Paging {
public:
    static constexpr std::uint64_t kPageSize = 0x1000;

    explicit Paging(Memory& physical_memory) noexcept;

    PagingResult Translate(
        std::uint64_t linear_address,
        bool write,
        bool user,
        bool instruction) const;

    void SetCr3(std::uint64_t value) noexcept;
    std::uint64_t Cr3() const noexcept;

    void SetCr4(std::uint64_t value) noexcept;
    std::uint64_t Cr4() const noexcept;

    void SetEfer(std::uint64_t value) noexcept;
    std::uint64_t Efer() const noexcept;

private:
    bool ReadEntry(std::uint64_t physical_address, std::uint64_t& value) const;
    static bool Canonical48(std::uint64_t address) noexcept;
    static std::uint64_t EntryAddress(std::uint64_t table, std::uint16_t index) noexcept;
    static PagingResult Fault(
        PagingFault reason,
        bool write,
        bool user,
        bool instruction) noexcept;

    Memory& memory_;
    std::uint64_t cr3_ = 0;
    std::uint64_t cr4_ = 0;
    std::uint64_t efer_ = 0;
};

} // namespace myps5emu
