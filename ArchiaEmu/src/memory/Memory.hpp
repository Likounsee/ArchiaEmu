#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace myps5emu {

class Memory {
public:
    virtual ~Memory() = default;

    virtual bool Map(std::uint64_t virtual_address,
                     std::size_t size);

    virtual bool Write(std::uint64_t virtual_address,
                       const std::uint8_t* data,
                       std::size_t size);

    virtual bool Read(std::uint64_t virtual_address,
                      std::uint8_t* data,
                      std::size_t size) const;

    virtual bool IsMapped(std::uint64_t virtual_address,
                          std::size_t size) const;

    virtual void Clear();

private:
    struct Region {
        std::uint64_t base = 0;
        std::vector<std::uint8_t> data;
    };

    std::vector<Region> regions_;
};

} // namespace myps5emu
