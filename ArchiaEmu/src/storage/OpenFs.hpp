#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace myps5emu::storage {

// OpenFS is the emulator's filesystem-neutral guest storage API.
// It deliberately does not pretend to be NTFS/ext/FAT compatible: concrete
// disk formats can be attached behind this interface later.
class OpenFs final {
public:
    OpenFs();

    bool CreateDirectory(const std::string& path);
    bool WriteFile(const std::string& path,
                   const std::vector<std::uint8_t>& data);
    bool ReadFile(const std::string& path,
                  std::vector<std::uint8_t>& data) const;
    bool Exists(const std::string& path) const;
    bool IsDirectory(const std::string& path) const;
    bool Remove(const std::string& path);

    std::vector<std::string> ListDirectory(
        const std::string& path) const;

private:
    struct Node {
        bool directory = false;
        std::vector<std::uint8_t> data;
    };

    static bool NormalizePath(const std::string& path,
                              std::string& normalized);
    static std::string ParentPath(const std::string& path);
    static std::string BaseName(const std::string& path);

    std::map<std::string, Node> nodes_;
};

} // namespace myps5emu::storage
