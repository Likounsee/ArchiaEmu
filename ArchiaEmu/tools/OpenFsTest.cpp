#include "storage/OpenFs.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using myps5emu::storage::OpenFs;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    OpenFs fs;

    if (!fs.Exists("/") || !fs.IsDirectory("/")) {
        return Fail("OpenFS root missing") ? 0 : 1;
    }

    if (!fs.CreateDirectory("/games") ||
        !fs.CreateDirectory("/games/demo") ||
        fs.CreateDirectory("/games/demo")) {
        return Fail("OpenFS directory semantics failed") ? 0 : 1;
    }

    const std::vector<std::uint8_t> payload{0x00, 0x01, 0xFE, 0xFF};
    if (!fs.WriteFile("/games/demo/save.bin", payload)) {
        return Fail("OpenFS file creation failed") ? 0 : 1;
    }

    std::vector<std::uint8_t> loaded;
    if (!fs.ReadFile("/games/demo/save.bin", loaded) ||
        loaded != payload) {
        return Fail("OpenFS round-trip failed") ? 0 : 1;
    }

    if (!fs.Exists("/games/demo/save.bin") ||
        fs.IsDirectory("/games/demo/save.bin") ||
        fs.Exists("/games/../escape")) {
        return Fail("OpenFS path validation failed") ? 0 : 1;
    }

    const auto entries = fs.ListDirectory("/games/demo");
    if (entries.size() != 1 || entries.front() != "save.bin") {
        return Fail("OpenFS directory listing failed") ? 0 : 1;
    }

    if (fs.Remove("/games/demo") ||
        !fs.Remove("/games/demo/save.bin") ||
        !fs.Remove("/games/demo") ||
        fs.Exists("/games/demo")) {
        return Fail("OpenFS removal semantics failed") ? 0 : 1;
    }

    std::cout << "OpenFS core semantics: PASS\n";
    return 0;
}
