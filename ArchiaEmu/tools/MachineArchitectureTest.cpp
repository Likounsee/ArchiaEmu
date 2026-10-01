#include "core/Bus.hpp"
#include "core/Device.hpp"
#include "core/Machine.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

class TestDevice final : public Device {
public:
    bool Read(std::uint64_t address,
              std::uint8_t* data,
              std::size_t size) const override
    {
        if (data == nullptr || size != 1 || address != 0) {
            return false;
        }
        *data = value_;
        return true;
    }

    bool Write(std::uint64_t address,
               const std::uint8_t* data,
               std::size_t size) override
    {
        if (data == nullptr || size != 1 || address != 0) {
            return false;
        }
        value_ = *data;
        return true;
    }

private:
    std::uint8_t value_ = 0xA5;
};

int main()
{
    Machine machine;

    if (machine.CPU().InstructionPointer() != 0) {
        std::cerr << "Machine CPU initialization failed\n";
        return 1;
    }

    if (!machine.GuestMemory().Map(0x400000, 0x1000)) {
        std::cerr << "Machine RAM mapping failed\n";
        return 1;
    }

    std::uint8_t ramWrite = 0x42;
    if (!machine.GuestMemory().Write(0x400000, &ramWrite, 1)) {
        std::cerr << "Machine RAM write failed\n";
        return 1;
    }

    std::uint8_t ramRead = 0;
    if (!machine.GuestMemory().Read(0x400000, &ramRead, 1) ||
        ramRead != ramWrite) {
        std::cerr << "Machine RAM read failed\n";
        return 1;
    }

    TestDevice device;
    auto* bus = dynamic_cast<Bus*>(&machine.GuestMemory());
    if (bus == nullptr) {
        std::cerr << "Machine is not backed by a Bus\n";
        return 1;
    }

    if (!bus->MapDevice(0x10000000, 0x100, &device)) {
        std::cerr << "Device mapping failed\n";
        return 1;
    }

    std::uint8_t deviceRead = 0;
    if (!bus->Read(0x10000000, &deviceRead, 1) ||
        deviceRead != 0xA5) {
        std::cerr << "Device read routing failed\n";
        return 1;
    }

    const std::uint8_t deviceWrite = 0x5A;
    if (!bus->Write(0x10000000, &deviceWrite, 1)) {
        std::cerr << "Device write routing failed\n";
        return 1;
    }

    if (!bus->Read(0x10000000, &deviceRead, 1) ||
        deviceRead != deviceWrite) {
        std::cerr << "Device write/readback failed\n";
        return 1;
    }

    if (bus->MapDevice(0x10000000, 0x10, &device)) {
        std::cerr << "Overlapping device mapping was accepted\n";
        return 1;
    }

    if (bus->MapDevice(0x400000, 0x10, &device)) {
        std::cerr << "Device/RAM overlap was accepted\n";
        return 1;
    }

    std::cout << "Machine bus/device architecture test: PASS\n";
    return 0;
}
