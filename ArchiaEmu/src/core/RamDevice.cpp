#include "RamDevice.hpp"

#include <cstring>
#include <limits>

namespace myps5emu {

RamDevice::RamDevice(std::size_t size)
    : data_(size, 0)
{
}

bool RamDevice::Read(std::uint64_t address,
                     std::uint8_t* data,
                     std::size_t size) const
{
    if (data == nullptr || size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    if (address > std::numeric_limits<std::uint64_t>::max() - size64) {
        return false;
    }

    if (address > std::numeric_limits<std::size_t>::max() ||
        address >= data_.size() ||
        size > data_.size() - static_cast<std::size_t>(address)) {
        return false;
    }

    std::memcpy(data, data_.data() + static_cast<std::size_t>(address), size);
    return true;
}

bool RamDevice::Write(std::uint64_t address,
                      const std::uint8_t* data,
                      std::size_t size)
{
    if (data == nullptr || size == 0) {
        return false;
    }

    const auto size64 = static_cast<std::uint64_t>(size);
    if (address > std::numeric_limits<std::uint64_t>::max() - size64) {
        return false;
    }

    if (address >= data_.size() ||
        size > data_.size() - static_cast<std::size_t>(address)) {
        return false;
    }

    std::memcpy(data_.data() + static_cast<std::size_t>(address), data, size);
    return true;
}

std::size_t RamDevice::Size() const noexcept
{
    return data_.size();
}

} // namespace myps5emu
