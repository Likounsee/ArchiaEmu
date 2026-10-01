#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Device.hpp"
#include "memory/Memory.hpp"

namespace myps5emu {

class Bus final : public Memory {
public:
    bool Map(std::uint64_t virtual_address,
             std::size_t size) override;

    bool Map(std::uint64_t virtual_address,
             std::size_t size,
             MemoryPermission permissions) override;

    bool MapDevice(std::uint64_t base,
                   std::size_t size,
                   Device* device,
                   MemoryPermission permissions =
                       MemoryPermission::Read | MemoryPermission::Write);

    bool UnmapDevice(Device* device) noexcept;

    bool Read(std::uint64_t virtual_address,
              std::uint8_t* data,
              std::size_t size) const override;

    bool Write(std::uint64_t virtual_address,
               const std::uint8_t* data,
               std::size_t size) override;

    bool ExecuteRead(std::uint64_t virtual_address,
                     std::uint8_t* data,
                     std::size_t size) const override;

    bool IsMapped(std::uint64_t virtual_address,
                  std::size_t size) const override;

    bool HasPermissionAt(std::uint64_t virtual_address,
                         std::size_t size,
                         MemoryPermission permission) const override;

    void ClearDevices() noexcept;
    void Clear() override;

private:
    struct DeviceMapping {
        std::uint64_t base = 0;
        std::size_t size = 0;
        Device* device = nullptr;
        MemoryPermission permissions =
            MemoryPermission::Read | MemoryPermission::Write;
    };

    const DeviceMapping* FindDevice(
        std::uint64_t address,
        std::size_t size) const noexcept;

    DeviceMapping* FindDevice(
        std::uint64_t address,
        std::size_t size) noexcept;

    std::vector<DeviceMapping> devices_;
};

} // namespace myps5emu
