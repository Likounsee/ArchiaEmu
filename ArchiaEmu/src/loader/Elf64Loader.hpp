#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace myps5emu {

struct LoadedSegment {
    std::uint64_t virtual_address = 0;
    std::uint64_t file_size = 0;
    std::uint64_t memory_size = 0;
    std::uint32_t flags = 0;
    std::vector<std::uint8_t> data;
};

class Elf64Loader {
public:
    bool Load(const std::string& path);

    std::uint64_t EntryPoint() const noexcept {
        return entry_point_;
    }

    const std::vector<LoadedSegment>& Segments() const noexcept {
        return segments_;
    }

private:
    std::uint64_t entry_point_ = 0;
    std::vector<LoadedSegment> segments_;
};

} // namespace myps5emu