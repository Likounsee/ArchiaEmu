#pragma once

#include "core/Device.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace myps5emu {

class MmioRegisterDevice final : public Device {
public:
    explicit MmioRegisterDevice(
        std::vector<std::uint32_t> initial_values,
        std::vector<std::uint32_t> write_masks = {});

    bool Read(std::uint64_t address,
              std::uint8_t* data,
              std::size_t size) const override;

    bool Write(std::uint64_t address,
               const std::uint8_t* data,
               std::size_t size) override;

    std::size_t Size() const noexcept;

    std::uint32_t ReadRegister(std::size_t index) const noexcept;

private:
    std::vector<std::uint32_t> values_;
    std::vector<std::uint32_t> write_masks_;
};

} // namespace myps5emu
