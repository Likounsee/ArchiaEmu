#include "MmioRegisterDevice.hpp"

#include <cstring>
#include <utility>

namespace myps5emu {

MmioRegisterDevice::MmioRegisterDevice(
    std::vector<std::uint32_t> initial_values,
    std::vector<std::uint32_t> write_masks)
    : values_(std::move(initial_values))
{
    if (write_masks.empty()) {
        write_masks_.assign(values_.size(), 0xFFFFFFFFU);
    } else if (write_masks.size() == values_.size()) {
        write_masks_ = std::move(write_masks);
    } else {
        values_.clear();
        write_masks_.clear();
    }
}

bool MmioRegisterDevice::Read(std::uint64_t address,
                              std::uint8_t* data,
                              std::size_t size) const
{
    if (data == nullptr || size != sizeof(std::uint32_t) ||
        address % sizeof(std::uint32_t) != 0) {
        return false;
    }

    const auto index = address / sizeof(std::uint32_t);
    if (index >= values_.size()) {
        return false;
    }

    const std::uint32_t value = values_[static_cast<std::size_t>(index)];
    std::memcpy(data, &value, sizeof(value));
    return true;
}

bool MmioRegisterDevice::Write(std::uint64_t address,
                               const std::uint8_t* data,
                               std::size_t size)
{
    if (data == nullptr || size != sizeof(std::uint32_t) ||
        address % sizeof(std::uint32_t) != 0) {
        return false;
    }

    const auto index = address / sizeof(std::uint32_t);
    if (index >= values_.size()) {
        return false;
    }

    std::uint32_t incoming = 0;
    std::memcpy(&incoming, data, sizeof(incoming));

    const auto register_index = static_cast<std::size_t>(index);
    const std::uint32_t mask = write_masks_[register_index];
    values_[register_index] =
        (values_[register_index] & ~mask) | (incoming & mask);
    return true;
}

std::size_t MmioRegisterDevice::Size() const noexcept
{
    return values_.size() * sizeof(std::uint32_t);
}

std::uint32_t MmioRegisterDevice::ReadRegister(std::size_t index) const noexcept
{
    if (index >= values_.size()) {
        return 0;
    }
    return values_[index];
}

} // namespace myps5emu
