#include "RomDevice.hpp"

#include <cstring>
#include <limits>
#include <utility>

namespace myps5emu {

RomDevice::RomDevice(std::vector<std::uint8_t> data)
    : data_(std::move(data))
{
}

bool RomDevice::Read(std::uint64_t address,
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

    if (address >= data_.size() ||
        size > data_.size() - static_cast<std::size_t>(address)) {
        return false;
    }

    std::memcpy(data, data_.data() + static_cast<std::size_t>(address), size);
    return true;
}

bool RomDevice::Write(std::uint64_t,
                      const std::uint8_t*,
                      std::size_t)
{
    return false;
}

std::size_t RomDevice::Size() const noexcept
{
    return data_.size();
}

} // namespace myps5emu
