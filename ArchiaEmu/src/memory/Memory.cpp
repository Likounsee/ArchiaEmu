#include "Memory.hpp"

#include <cstring>
#include <limits>
#include <utility>

namespace myps5emu {

namespace {

bool RangeValid(std::uint64_t address, std::size_t size) noexcept
{
    if (size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    return size64 <= std::numeric_limits<std::uint64_t>::max() - address;
}

} // namespace

bool Memory::Map(std::uint64_t virtual_address, std::size_t size)
{
    return Map(virtual_address, size,
               MemoryPermission::Read | MemoryPermission::Write |
               MemoryPermission::Execute);
}

bool Memory::Map(std::uint64_t virtual_address,
                 std::size_t size,
                 MemoryPermission permissions)
{
    if (!RangeValid(virtual_address, size)) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    // Mappings are page-based: callers must describe complete pages.
    if ((virtual_address % PageSize) != 0 || (size % PageSize) != 0) {
        SetFault(MemoryFault::Unaligned);
        return false;
    }

    if (HasOverlappingRegion(virtual_address, size)) {
        SetFault(MemoryFault::Overlap);
        return false;
    }

    Region region;
    region.base = virtual_address;
    region.data.resize(size, 0);
    region.permissions = permissions;
    regions_.push_back(std::move(region));
    SetFault(MemoryFault::None);
    return true;
}

bool Memory::IsMapped(std::uint64_t virtual_address,
                      std::size_t size) const
{
    return HasPermissionAt(virtual_address, size, MemoryPermission::None);
}

bool Memory::HasPermissionAt(std::uint64_t virtual_address,
                             std::size_t size,
                             MemoryPermission permission) const
{
    if (!RangeValid(virtual_address, size)) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    const std::uint64_t end = virtual_address + size64;

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base && end <= region_end) {
            return permission == MemoryPermission::None ||
                   HasPermission(region.permissions, permission);
        }
    }

    return false;
}

bool Memory::Write(std::uint64_t virtual_address,
                   const std::uint8_t* data,
                   std::size_t size)
{
    if (data == nullptr || !RangeValid(virtual_address, size)) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) {
        SetFault(MemoryFault::Unmapped);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Write)) {
        SetFault(MemoryFault::PermissionDenied);
        return false;
    }

    for (auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            virtual_address + static_cast<std::uint64_t>(size) <= region_end) {
            const std::size_t offset =
                static_cast<std::size_t>(virtual_address - region.base);
            std::memcpy(region.data.data() + offset, data, size);
            SetFault(MemoryFault::None);
            return true;
        }
    }

    return false;
}

bool Memory::Read(std::uint64_t virtual_address,
                  std::uint8_t* data,
                  std::size_t size) const
{
    if (data == nullptr || !RangeValid(virtual_address, size)) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) {
        SetFault(MemoryFault::Unmapped);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Read)) {
        SetFault(MemoryFault::PermissionDenied);
        return false;
    }

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            virtual_address + static_cast<std::uint64_t>(size) <= region_end) {
            const std::size_t offset =
                static_cast<std::size_t>(virtual_address - region.base);
            std::memcpy(data, region.data.data() + offset, size);
            SetFault(MemoryFault::None);
            return true;
        }
    }

    return false;
}

bool Memory::ExecuteRead(std::uint64_t virtual_address,
                         std::uint8_t* data,
                         std::size_t size) const
{
    if (data == nullptr || !RangeValid(virtual_address, size)) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) {
        SetFault(MemoryFault::Unmapped);
        return false;
    }

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::Execute)) {
        SetFault(MemoryFault::PermissionDenied);
        return false;
    }

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            virtual_address + static_cast<std::uint64_t>(size) <= region_end) {
            const std::size_t offset =
                static_cast<std::size_t>(virtual_address - region.base);
            std::memcpy(data, region.data.data() + offset, size);
            SetFault(MemoryFault::None);
            return true;
        }
    }

    return false;
}

MemoryFault Memory::LastFault() const noexcept
{
    return last_fault_;
}

void Memory::SetFault(MemoryFault fault) const noexcept
{
    last_fault_ = fault;
}

bool Memory::HasOverlappingRegion(std::uint64_t virtual_address,
                                  std::size_t size) const noexcept
{
    if (!RangeValid(virtual_address, size)) {
        return true;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    const std::uint64_t end = virtual_address + size64;

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());
        if (virtual_address < region_end && end > region.base) {
            return true;
        }
    }

    return false;
}

bool Memory::Protect(std::uint64_t virtual_address,
                     std::size_t size,
                     MemoryPermission permissions)
{
    if (!RangeValid(virtual_address, size)) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    const std::uint64_t end = virtual_address + size64;

    if ((virtual_address % PageSize) != 0 ||
        (size % PageSize) != 0) {
        SetFault(MemoryFault::Unaligned);
        return false;
    }

    for (auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base && end <= region_end) {
            region.permissions = permissions;
            SetFault(MemoryFault::None);
            return true;
        }
    }

    return false;
}

void Memory::Clear()
{
    regions_.clear();
    SetFault(MemoryFault::None);
}

} // namespace myps5emu
