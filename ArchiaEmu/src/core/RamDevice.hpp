#pragma once

#include "core/Device.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace myps5emu {

class RamDevice final : public Device {
public:
    explicit RamDevice(std::size_t size);

    bool Read(std::uint64_t address,
              std::uint8_t* data,
              std::size_t size) const override;

    bool Write(std::uint64_t address,
               const std::uint8_t* data,
               std::size_t size) override;

    std::size_t Size() const noexcept;

private:
    std::vector<std::uint8_t> data_;
};

} // namespace myps5emu
