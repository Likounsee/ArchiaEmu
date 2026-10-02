#include "Memory.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

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

const Memory::Region* FindRegion(const std::vector<Memory::Region>& regions,
                                 std::uint64_t address) noexcept
{
    for (const auto& region : regions) {
        if (address >= region.base &&
            address - region.base < static_cast<std::uint64_t>(region.size)) {
            return &region;
        }
    }
    return nullptr;
}

Memory::Region* FindRegion(std::vector<Memory::Region>& regions,
                           std::uint64_t address) noexcept
{
    for (auto& region : regions) {
        if (address >= region.base &&
            address - region.base < static_cast<std::uint64_t>(region.size)) {
            return &region;
        }
    }
    return nullptr;
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
    region.size = size;
    region.permissions = permissions;
    regions_.push_back(region);
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

    std::uint64_t cursor = virtual_address;
    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    while (cursor < end) {
        const Region* region = FindRegion(regions_, cursor);
        if (region == nullptr) {
            return false;
        }

        const std::uint64_t region_end =
            region->base + static_cast<std::uint64_t>(region->size);

        if (permission != MemoryPermission::None &&
            !HasPermission(region->permissions, permission)) {
            return false;
        }

        cursor = std::min(region_end, end);
    }

    return true;
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

    std::uint64_t cursor = virtual_address;
    std::size_t source_offset = 0;
    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    while (cursor < end) {
        const std::size_t page_offset =
            static_cast<std::size_t>(cursor % PageSize);
        const std::size_t chunk =
            std::min(PageSize - page_offset,
                     static_cast<std::size_t>(end - cursor));

        const std::uint64_t page_base =
            cursor - static_cast<std::uint64_t>(page_offset);
        auto& page = pages_[page_base];

        std::memcpy(page.data() + page_offset,
                    data + source_offset,
                    chunk);

        cursor += chunk;
        source_offset += chunk;
    }

    SetFault(MemoryFault::None);
    return true;
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

    std::uint64_t cursor = virtual_address;
    std::size_t destination_offset = 0;
    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    while (cursor < end) {
        const std::size_t page_offset =
            static_cast<std::size_t>(cursor % PageSize);
        const std::size_t chunk =
            std::min(PageSize - page_offset,
                     static_cast<std::size_t>(end - cursor));

        const std::uint64_t page_base =
            cursor - static_cast<std::uint64_t>(page_offset);
        const auto it = pages_.find(page_base);

        if (it == pages_.end()) {
            std::memset(data + destination_offset, 0, chunk);
        } else {
            std::memcpy(data + destination_offset,
                        it->second.data() + page_offset,
                        chunk);
        }

        cursor += chunk;
        destination_offset += chunk;
    }

    SetFault(MemoryFault::None);
    return true;
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

    std::uint64_t cursor = virtual_address;
    std::size_t destination_offset = 0;
    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    while (cursor < end) {
        const std::size_t page_offset =
            static_cast<std::size_t>(cursor % PageSize);
        const std::size_t chunk =
            std::min(PageSize - page_offset,
                     static_cast<std::size_t>(end - cursor));

        const std::uint64_t page_base =
            cursor - static_cast<std::uint64_t>(page_offset);
        const auto it = pages_.find(page_base);

        if (it == pages_.end()) {
            std::memset(data + destination_offset, 0, chunk);
        } else {
            std::memcpy(data + destination_offset,
                        it->second.data() + page_offset,
                        chunk);
        }

        cursor += chunk;
        destination_offset += chunk;
    }

    SetFault(MemoryFault::None);
    return true;
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

    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.size);

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
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if ((virtual_address % PageSize) != 0 ||
        (size % PageSize) != 0) {
        SetFault(MemoryFault::Unaligned);
        return false;
    }

    const std::uint64_t end =
        virtual_address + static_cast<std::uint64_t>(size);

    if (!HasPermissionAt(virtual_address, size, MemoryPermission::None)) {
        SetFault(MemoryFault::Unmapped);
        return false;
    }

    for (std::size_t i = 0; i < regions_.size();) {
        const Region region = regions_[i];
        const std::uint64_t region_end =
            region.base + static_cast<std::uint64_t>(region.size);

        const std::uint64_t begin =
            std::max(region.base, virtual_address);
        const std::uint64_t finish =
            std::min(region_end, end);

        if (begin >= finish) {
            ++i;
            continue;
        }

        std::vector<Region> replacement;

        if (begin > region.base) {
            replacement.push_back(
                Region{region.base,
                       static_cast<std::size_t>(begin - region.base),
                       region.permissions});
        }

        replacement.push_back(
            Region{begin,
                   static_cast<std::size_t>(finish - begin),
                   permissions});

        if (finish < region_end) {
            replacement.push_back(
                Region{finish,
                       static_cast<std::size_t>(region_end - finish),
                       region.permissions});
        }

        regions_.erase(
            regions_.begin() + static_cast<std::ptrdiff_t>(i));
        regions_.insert(
            regions_.begin() + static_cast<std::ptrdiff_t>(i),
            replacement.begin(),
            replacement.end());

        i += replacement.size();
    }

    SetFault(MemoryFault::None);
    return true;
}

void Memory::Clear()
{
    regions_.clear();
    pages_.clear();
    SetFault(MemoryFault::None);
}

} // namespace myps5emu
