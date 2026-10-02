#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <unordered_map>
#include <vector>

namespace myps5emu {

enum class MemoryFault : std::uint8_t {
    None,
    InvalidRange,
    Unaligned,
    Overlap,
    Unmapped,
    PermissionDenied,
    DeviceRejected
};

enum class MemoryPermission : std::uint8_t {
    None = 0,
    Read = 1U << 0,
    Write = 1U << 1,
    Execute = 1U << 2
};

constexpr MemoryPermission operator|(MemoryPermission lhs,
                                     MemoryPermission rhs) noexcept
{
    return static_cast<MemoryPermission>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs));
}

constexpr bool HasPermission(MemoryPermission permissions,
                              MemoryPermission requested) noexcept
{
    return (static_cast<std::uint8_t>(permissions) &
            static_cast<std::uint8_t>(requested)) ==
           static_cast<std::uint8_t>(requested);
}

class Memory {
public:
    static constexpr std::size_t PageSize = 0x1000;

    virtual ~Memory() = default;

    virtual bool Map(std::uint64_t virtual_address,
                     std::size_t size);

    virtual bool Map(std::uint64_t virtual_address,
                     std::size_t size,
                     MemoryPermission permissions);

    virtual bool Write(std::uint64_t virtual_address,
                       const std::uint8_t* data,
                       std::size_t size);

    virtual bool Read(std::uint64_t virtual_address,
                      std::uint8_t* data,
                      std::size_t size) const;

    virtual bool ExecuteRead(std::uint64_t virtual_address,
                             std::uint8_t* data,
                             std::size_t size) const;

    virtual bool IsMapped(std::uint64_t virtual_address,
                          std::size_t size) const;

    virtual bool HasPermissionAt(std::uint64_t virtual_address,
                                 std::size_t size,
                                 MemoryPermission permission) const;

    virtual bool Protect(std::uint64_t virtual_address,
                         std::size_t size,
                         MemoryPermission permissions);

    virtual void Clear();

    MemoryFault LastFault() const noexcept;

protected:
    void SetFault(MemoryFault fault) const noexcept;
    bool HasOverlappingRegion(std::uint64_t virtual_address,
                              std::size_t size) const noexcept;

private:
    struct Region {
        std::uint64_t base = 0;
        std::size_t size = 0;
        MemoryPermission permissions =
            MemoryPermission::Read | MemoryPermission::Write;
    };

    std::vector<Region> regions_;
    std::unordered_map<std::uint64_t, std::array<std::uint8_t, PageSize>> pages_;
    mutable MemoryFault last_fault_ = MemoryFault::None;
};

} // namespace myps5emu
