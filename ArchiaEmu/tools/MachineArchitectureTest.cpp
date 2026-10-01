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

    const std::uint8_t ramWrite = 0x42;
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
    Bus& bus = machine.SystemBus();

    if (!bus.MapDevice(0x10000000, 0x100, &device)) {
        std::cerr << "Device mapping failed\n";
        return 1;
    }

    std::uint8_t deviceRead = 0;
    if (!bus.Read(0x10000000, &deviceRead, 1) ||
        deviceRead != 0xA5) {
        std::cerr << "Device read routing failed\n";
        return 1;
    }

    const std::uint8_t deviceWrite = 0x5A;
    if (!bus.Write(0x10000000, &deviceWrite, 1) ||
        !bus.Read(0x10000000, &deviceRead, 1) ||
        deviceRead != deviceWrite) {
        std::cerr << "Device write/readback failed\n";
        return 1;
    }

    if (bus.MapDevice(0x10000000, 0x10, &device)) {
        std::cerr << "Overlapping device mapping was accepted\n";
        return 1;
    }

    if (bus.MapDevice(0x400000, 0x10, &device)) {
        std::cerr << "Device/RAM overlap was accepted\n";
        return 1;
    }

    if (bus.Map(0x10000050, 0x10)) {
        std::cerr << "RAM/device overlap was accepted\n";
        return 1;
    }

    const std::uint8_t program[] = {
        0xB8, 0x78, 0x56, 0x34, 0x12,
        0xF4
    };

    if (!bus.Write(0x400000, program, sizeof(program))) {
        std::cerr << "CPU program write failed\n";
        return 1;
    }

    machine.CPU().SetInstructionPointer(0x400000);
    if (machine.CPU().Run() != 0 ||
        machine.CPU().ReadRegister64(0) != 0x12345678ULL) {
        std::cerr << "CPU did not execute through Bus-backed memory\n";
        return 1;
    }

    bus.Clear();
    if (bus.IsMapped(0x400000, 1)) {
        std::cerr << "Bus RAM was not cleared\n";
        return 1;
    }

    std::cout << "Machine bus/device architecture test: PASS\n";
    return 0;
}
