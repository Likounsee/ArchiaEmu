#include "Memory.hpp"

#include <cstring>
#include <limits>
#include <utility>

namespace myps5emu {

bool Memory::Map(std::uint64_t virtual_address,
                 std::size_t size)
{
    if (size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);

    if (size64 > std::numeric_limits<std::uint64_t>::max() -
                     virtual_address) {
        return false;
    }

    const std::uint64_t end = virtual_address + size64;

    // Refuse les chevauchements pour le moment.
    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base +
            static_cast<std::uint64_t>(region.data.size());

        if (virtual_address < region_end &&
            end > region.base) {
            return false;
        }
    }

    Region region;
    region.base = virtual_address;
    region.data.resize(size, 0);

    regions_.push_back(std::move(region));
    return true;
}

bool Memory::IsMapped(std::uint64_t virtual_address,
                      std::size_t size) const
{
    if (size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);

    if (size64 > std::numeric_limits<std::uint64_t>::max() -
                     virtual_address) {
        return false;
    }

    const std::uint64_t end = virtual_address + size64;

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base +
            static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            end <= region_end) {
            return true;
        }
    }

    return false;
}

bool Memory::Write(std::uint64_t virtual_address,
                   const std::uint8_t* data,
                   std::size_t size)
{
    if (data == nullptr || !IsMapped(virtual_address, size)) {
        return false;
    }

    for (auto& region : regions_) {
        const std::uint64_t region_end =
            region.base +
            static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            virtual_address +
                    static_cast<std::uint64_t>(size) <=
                region_end) {

            const std::size_t offset =
                static_cast<std::size_t>(
                    virtual_address - region.base);

            std::memcpy(
                region.data.data() + offset,
                data,
                size);

            return true;
        }
    }

    return false;
}

bool Memory::Read(std::uint64_t virtual_address,
                  std::uint8_t* data,
                  std::size_t size) const
{
    if (data == nullptr || !IsMapped(virtual_address, size)) {
        return false;
    }

    for (const auto& region : regions_) {
        const std::uint64_t region_end =
            region.base +
            static_cast<std::uint64_t>(region.data.size());

        if (virtual_address >= region.base &&
            virtual_address +
                    static_cast<std::uint64_t>(size) <=
                region_end) {

            const std::size_t offset =
                static_cast<std::size_t>(
                    virtual_address - region.base);

            std::memcpy(
                data,
                region.data.data() + offset,
                size);

            return true;
        }
    }

    return false;
}

bool Memory::HasOverlappingRegion(std::uint64_t virtual_address,
                                  std::size_t size) const noexcept
{
    if (size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    if (size64 > std::numeric_limits<std::uint64_t>::max() - virtual_address) {
        return true;
    }

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

void Memory::Clear()
{
    regions_.clear();
}

} // namespace myps5emu