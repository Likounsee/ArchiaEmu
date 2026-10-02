#include "core/Bus.hpp"
#include "core/RamDevice.hpp"
#include "core/RomDevice.hpp"
#include "core/MmioRegisterDevice.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace myps5emu;

int main()
{
    Bus bus;
    RamDevice ram(16);
    RomDevice rom({0x11, 0x22, 0x33, 0x44});
    MmioRegisterDevice mmio({0xAABBCCDDU}, {0x0000FFFFU});

    if (!bus.MapDevice(0x1000, ram.Size(), &ram,
                       MemoryPermission::Read | MemoryPermission::Write)) {
        return 1;
    }
    std::array<std::uint8_t, 2> value{0x12, 0x34};
    if (!bus.Write(0x1002, value.data(), value.size())) return 2;

    std::array<std::uint8_t, 2> read{};
    if (!bus.Read(0x1002, read.data(), read.size()) || read != value) return 3;

    if (bus.ExecuteRead(0x1002, read.data(), read.size())) return 4;
    if (bus.LastFault() != MemoryFault::PermissionDenied) return 5;

    if (!bus.MapDevice(0x3000, rom.Size(), &rom, MemoryPermission::Read)) return 6;
    if (!bus.Read(0x3000, read.data(), read.size())) return 7;
    if (read[0] != 0x11 || read[1] != 0x22) return 8;
    if (bus.Write(0x3000, value.data(), value.size())) return 9;

    if (!bus.MapDevice(0x2000, mmio.Size(), &mmio,
                       MemoryPermission::Read | MemoryPermission::Write)) return 10;
    const std::array<std::uint8_t, 4> mmioWrite{0x34, 0x12, 0x00, 0x00};
    if (!bus.Write(0x2000, mmioWrite.data(), mmioWrite.size())) return 11;
    std::array<std::uint8_t, 4> mmioRead{};
    if (!bus.Read(0x2000, mmioRead.data(), mmioRead.size())) return 12;
    if (mmioRead[0] != 0x34 || mmioRead[1] != 0x12 ||
        mmioRead[2] != 0xBB || mmioRead[3] != 0xAA) return 13;

    if (bus.MapDevice(0x1004, 4, &ram, MemoryPermission::Read)) return 14;
    if (bus.UnmapDevice(&rom) == false) return 15;
    if (bus.Read(0x3000, read.data(), read.size())) return 16;

    std::cout << "Bus device mapping/permissions test: PASS\n";
    return 0;
}
