#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "memory/Memory.hpp"
#include "core/Device.hpp"

namespace myps5emu {

class Bus final : public Memory {
public:
    bool MapDevice(std::uint64_t base,
                   std::size_t size,
                   Device* device);

    bool UnmapDevice(Device* device) noexcept;

    bool Read(std::uint64_t virtual_address,
              std::uint8_t* data,
              std::size_t size) const override;

    bool Write(std::uint64_t virtual_address,
               const std::uint8_t* data,
               std::size_t size) override;

    void ClearDevices() noexcept;

private:
    struct DeviceMapping {
        std::uint64_t base = 0;
        std::size_t size = 0;
        Device* device = nullptr;
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
