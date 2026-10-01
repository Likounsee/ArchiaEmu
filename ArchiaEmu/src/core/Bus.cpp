#include "Bus.hpp"

#include <limits>

namespace myps5emu {

bool Bus::MapDevice(std::uint64_t base,
                    std::size_t size,
                    Device* device)
{
    if (device == nullptr || size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    if (size64 > std::numeric_limits<std::uint64_t>::max() - base) {
        return false;
    }

    const std::uint64_t end = base + size64;

    for (const auto& mapping : devices_) {
        const std::uint64_t mapping_end =
            mapping.base + static_cast<std::uint64_t>(mapping.size);

        if (base < mapping_end && end > mapping.base) {
            return false;
        }
    }

    if (IsMapped(base, size)) {
        return false;
    }

    devices_.push_back(DeviceMapping{base, size, device});
    return true;
}

bool Bus::UnmapDevice(Device* device) noexcept
{
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
        if (it->device == device) {
            devices_.erase(it);
            return true;
        }
    }
    return false;
}

const Bus::DeviceMapping* Bus::FindDevice(
    std::uint64_t address,
    std::size_t size) const noexcept
{
    if (size == 0) {
        return nullptr;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    if (size64 > std::numeric_limits<std::uint64_t>::max() - address) {
        return nullptr;
    }

    const std::uint64_t end = address + size64;

    for (const auto& mapping : devices_) {
        const std::uint64_t mapping_end =
            mapping.base + static_cast<std::uint64_t>(mapping.size);

        if (address >= mapping.base && end <= mapping_end) {
            return &mapping;
        }
    }

    return nullptr;
}

Bus::DeviceMapping* Bus::FindDevice(
    std::uint64_t address,
    std::size_t size) noexcept
{
    return const_cast<DeviceMapping*>(
        static_cast<const Bus*>(this)->FindDevice(address, size));
}

bool Bus::Read(std::uint64_t virtual_address,
               std::uint8_t* data,
               std::size_t size) const
{
    if (const auto* mapping = FindDevice(virtual_address, size)) {
        const std::uint64_t offset = virtual_address - mapping->base;
        return mapping->device->Read(offset, data, size);
    }

    return Memory::Read(virtual_address, data, size);
}

bool Bus::Write(std::uint64_t virtual_address,
                const std::uint8_t* data,
                std::size_t size)
{
    if (auto* mapping = FindDevice(virtual_address, size)) {
        const std::uint64_t offset = virtual_address - mapping->base;
        return mapping->device->Write(offset, data, size);
    }

    return Memory::Write(virtual_address, data, size);
}

void Bus::ClearDevices() noexcept
{
    devices_.clear();
}

} // namespace myps5emu
