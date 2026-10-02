#include "Bus.hpp"

#include <limits>

namespace myps5emu {

bool Bus::Map(std::uint64_t virtual_address,
              std::size_t size)
{
    return Map(virtual_address, size,
               MemoryPermission::Read | MemoryPermission::Write |
               MemoryPermission::Execute);
}

bool Bus::Map(std::uint64_t virtual_address,
              std::size_t size,
              MemoryPermission permissions)
{
    if (size == 0) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }
    const auto size64 = static_cast<std::uint64_t>(size);
    if (size64 > std::numeric_limits<std::uint64_t>::max() - virtual_address) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if (HasOverlappingRegion(virtual_address, size)) {
        SetFault(MemoryFault::Overlap);
        return false;
    }

    const std::uint64_t end = virtual_address + size64;

    for (const auto& mapping : devices_) {
        const std::uint64_t mapping_end =
            mapping.base + static_cast<std::uint64_t>(mapping.size);

        if (virtual_address < mapping_end && end > mapping.base) {
            SetFault(MemoryFault::Overlap);
            return false;
        }
    }

    return Memory::Map(virtual_address, size, permissions);
}

bool Bus::MapDevice(std::uint64_t base,
                    std::size_t size,
                    Device* device,
                    MemoryPermission permissions)
{
    if (device == nullptr || size == 0) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }
    const auto size64 = static_cast<std::uint64_t>(size);
    if (size64 > std::numeric_limits<std::uint64_t>::max() - base) {
        SetFault(MemoryFault::InvalidRange);
        return false;
    }

    if (HasOverlappingRegion(base, size)) {
        SetFault(MemoryFault::Overlap);
        return false;
    }

    const std::uint64_t end = base + size64;

    for (const auto& mapping : devices_) {
        const std::uint64_t mapping_end =
            mapping.base + static_cast<std::uint64_t>(mapping.size);

        if (base < mapping_end && end > mapping.base) {
            SetFault(MemoryFault::Overlap);
            return false;
        }
    }

    devices_.push_back(DeviceMapping{base, size, device, permissions});
    SetFault(MemoryFault::None);
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
        if (!HasPermission(mapping->permissions, MemoryPermission::Read)) {
            SetFault(MemoryFault::PermissionDenied);
            return false;
        }
        const bool success = mapping->device->Read(offset, data, size);
        SetFault(success ? MemoryFault::None : MemoryFault::DeviceRejected);
        return success;
    }

    return Memory::Read(virtual_address, data, size);
}

bool Bus::Write(std::uint64_t virtual_address,
                const std::uint8_t* data,
                std::size_t size)
{
    if (auto* mapping = FindDevice(virtual_address, size)) {
        const std::uint64_t offset = virtual_address - mapping->base;
        if (!HasPermission(mapping->permissions, MemoryPermission::Write)) {
            SetFault(MemoryFault::PermissionDenied);
            return false;
        }
        const bool success = mapping->device->Write(offset, data, size);
        SetFault(success ? MemoryFault::None : MemoryFault::DeviceRejected);
        return success;
    }

    return Memory::Write(virtual_address, data, size);
}

bool Bus::ExecuteRead(std::uint64_t virtual_address,
                      std::uint8_t* data,
                      std::size_t size) const
{
    if (const auto* mapping = FindDevice(virtual_address, size)) {
        if (!HasPermission(mapping->permissions, MemoryPermission::Execute)) {
            SetFault(MemoryFault::PermissionDenied);
            return false;
        }
        const std::uint64_t offset = virtual_address - mapping->base;
        const bool success = mapping->device->Read(offset, data, size);
        SetFault(success ? MemoryFault::None : MemoryFault::DeviceRejected);
        return success;
    }

    return Memory::ExecuteRead(virtual_address, data, size);
}

bool Bus::IsMapped(std::uint64_t virtual_address,
                    std::size_t size) const
{
    return HasPermissionAt(virtual_address, size, MemoryPermission::None);
}

bool Bus::HasPermissionAt(std::uint64_t virtual_address,
                          std::size_t size,
                          MemoryPermission permission) const
{
    if (const auto* mapping = FindDevice(virtual_address, size)) {
        return permission == MemoryPermission::None ||
               HasPermission(mapping->permissions, permission);
    }

    return Memory::HasPermissionAt(virtual_address, size, permission);
}

void Bus::ClearDevices() noexcept
{
    devices_.clear();
}

void Bus::Clear()
{
    devices_.clear();
    Memory::Clear();
}

} // namespace myps5emu
