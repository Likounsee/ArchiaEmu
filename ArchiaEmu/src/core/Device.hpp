#pragma once

#include <cstddef>
#include <cstdint>

namespace myps5emu {

class Device {
public:
    virtual ~Device() = default;

    virtual bool Read(std::uint64_t address,
                      std::uint8_t* data,
                      std::size_t size) const = 0;

    virtual bool Write(std::uint64_t address,
                       const std::uint8_t* data,
                       std::size_t size) = 0;
};

} // namespace myps5emu
