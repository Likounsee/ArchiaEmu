#include "Paging.hpp"

#include "memory/Memory.hpp"

namespace myps5emu {

namespace {
constexpr std::uint64_t kPresent = 1ULL << 0;
constexpr std::uint64_t kWrite = 1ULL << 1;
constexpr std::uint64_t kUser = 1ULL << 2;
constexpr std::uint64_t kLarge = 1ULL << 7;
constexpr std::uint64_t kNx = 1ULL << 63;
constexpr std::uint64_t kCr4Pae = 1ULL << 5;
constexpr std::uint64_t kEferNxe = 1ULL << 11;
constexpr std::uint32_t kPfWrite = 1U << 1;
constexpr std::uint32_t kPfUser = 1U << 2;
constexpr std::uint32_t kPfReserved = 1U << 3;
constexpr std::uint32_t kPfInstruction = 1U << 4;

std::uint32_t ErrorBits(bool write, bool user, bool instruction) noexcept
{
    return (write ? kPfWrite : 0U) |
           (user ? kPfUser : 0U) |
           (instruction ? kPfInstruction : 0U);
}
} // namespace

Paging::Paging(Memory& physical_memory) noexcept
    : memory_(physical_memory)
{
}

bool Paging::Canonical48(std::uint64_t address) noexcept
{
    const std::uint64_t upper = address >> 48;
    const bool sign = (address & (1ULL << 47)) != 0;
    return sign ? upper == 0xFFFFU : upper == 0;
}

std::uint64_t Paging::EntryAddress(
    std::uint64_t table,
    std::uint16_t index) noexcept
{
    return (table & 0x000FFFFFFFFFF000ULL) +
           static_cast<std::uint64_t>(index) * 8ULL;
}

bool Paging::ReadEntry(
    std::uint64_t physical_address,
    std::uint64_t& value) const
{
    return memory_.Read(
        physical_address,
        reinterpret_cast<std::uint8_t*>(&value),
        sizeof(value));
}

PagingResult Paging::Fault(
    PagingFault reason,
    bool write,
    bool user,
    bool instruction) noexcept
{
    return {
        false,
        0,
        reason,
        ErrorBits(write, user, instruction) |
            (reason != PagingFault::NotPresent ? 1U : 0U)
    };
}

PagingResult Paging::Translate(
    std::uint64_t linear_address,
    bool write,
    bool user,
    bool instruction) const
{
    if (!Canonical48(linear_address) ||
        (cr4_ & kCr4Pae) == 0 ||
        (cr3_ & 0xFFFULL) != 0) {
        return Fault(PagingFault::Malformed, write, user, instruction);
    }

    const std::uint64_t indexes[] = {
        (linear_address >> 39) & 0x1FFULL,
        (linear_address >> 30) & 0x1FFULL,
        (linear_address >> 21) & 0x1FFULL,
        (linear_address >> 12) & 0x1FFULL
    };

    std::uint64_t table = cr3_ & 0x000FFFFFFFFFF000ULL;
    bool effective_write = true;
    bool effective_user = true;
    bool nx = false;

    for (std::size_t level = 0; level < 4; ++level) {
        std::uint64_t entry = 0;
        if (!ReadEntry(EntryAddress(table, static_cast<std::uint16_t>(indexes[level])), entry)) {
            return Fault(PagingFault::NotPresent, write, user, instruction);
        }

        // Bits 52..62 are reserved in the modeled page-table format.
        // NX (bit 63) is reserved until EFER.NXE is enabled.
        if ((entry & 0x7FF0000000000000ULL) != 0 ||
            ((entry & kNx) != 0 && (efer_ & kEferNxe) == 0)) {
            return Fault(PagingFault::Reserved, write, user, instruction);
        }

        if ((entry & kPresent) == 0) {
            return Fault(PagingFault::NotPresent, write, user, instruction);
        }

        effective_write = effective_write && (entry & kWrite) != 0;
        effective_user = effective_user && (entry & kUser) != 0;
        nx = nx || (entry & kNx) != 0;

        if (level == 0 && (entry & kLarge) != 0) {
            return Fault(PagingFault::Reserved, write, user, instruction);
        }

        if (level == 1 && (entry & kLarge) != 0) {
            if ((entry & 0x00000000001FE000ULL) != 0) {
                return Fault(PagingFault::Reserved, write, user, instruction);
            }
            if (instruction && nx && (efer_ & kEferNxe) != 0) {
                return Fault(PagingFault::Instruction, write, user, instruction);
            }
            if (user && !effective_user) {
                return Fault(PagingFault::User, write, user, instruction);
            }
            if (write && !effective_write &&
                (user || (cr0_ & (1ULL << 16)) != 0)) {
                return Fault(PagingFault::Write, write, user, instruction);
            }
            return {true, (entry & 0x000FFFFFC0000000ULL) |
                            (linear_address & 0x3FFFFFFFULL),
                    PagingFault::None, 0};
        }

        if (level == 2 && (entry & kLarge) != 0) {
            if ((entry & 0x00000000001FE000ULL) != 0) {
                return Fault(PagingFault::Reserved, write, user, instruction);
            }
            if (instruction && nx && (efer_ & kEferNxe) != 0) {
                return Fault(PagingFault::Instruction, write, user, instruction);
            }
            if (user && !effective_user) {
                return Fault(PagingFault::User, write, user, instruction);
            }
            if (write && !effective_write &&
                (user || (cr0_ & (1ULL << 16)) != 0)) {
                return Fault(PagingFault::Write, write, user, instruction);
            }
            return {true, (entry & 0x000FFFFFFFE00000ULL) |
                            (linear_address & 0x1FFFFFULL),
                    PagingFault::None, 0};
        }

        if (level == 3) {
            if (instruction && nx && (efer_ & kEferNxe) != 0) {
                return Fault(PagingFault::Instruction, write, user, instruction);
            }
            if (user && !effective_user) {
                return Fault(PagingFault::User, write, user, instruction);
            }
            if (write && !effective_write &&
                (user || (cr0_ & (1ULL << 16)) != 0)) {
                return Fault(PagingFault::Write, write, user, instruction);
            }
            return {true, (entry & 0x000FFFFFFFFFF000ULL) |
                            (linear_address & 0xFFFULL),
                    PagingFault::None, 0};
        }

        table = entry & 0x000FFFFFFFFFF000ULL;
    }

    return Fault(PagingFault::Malformed, write, user, instruction);
}

void Paging::SetCr0(std::uint64_t value) noexcept { cr0_ = value; }
std::uint64_t Paging::Cr0() const noexcept { return cr0_; }

void Paging::SetCr3(std::uint64_t value) noexcept { cr3_ = value; }
std::uint64_t Paging::Cr3() const noexcept { return cr3_; }
void Paging::SetCr4(std::uint64_t value) noexcept { cr4_ = value; }
std::uint64_t Paging::Cr4() const noexcept { return cr4_; }
void Paging::SetEfer(std::uint64_t value) noexcept { efer_ = value; }
std::uint64_t Paging::Efer() const noexcept { return efer_; }

} // namespace myps5emu
