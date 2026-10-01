#include "core/Machine.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;

int main()
{
    Machine machine;

    const bool cpu_connected =
        machine.CPU().InstructionPointer() == 0;

    const bool memory_starts_empty =
        !machine.Memory().IsMapped(0x400000, 1);

    if (!cpu_connected || !memory_starts_empty) {
        std::cerr << "Machine architecture test failed\n";
        return 1;
    }

    if (!machine.Memory().Map(0x400000, 0x1000)) {
        std::cerr << "Machine memory mapping failed\n";
        return 1;
    }

    std::uint8_t value = 0x42;
    if (!machine.Memory().Write(
            0x400000, &value, sizeof(value))) {
        std::cerr << "Machine memory write failed\n";
        return 1;
    }

    std::uint8_t read = 0;
    if (!machine.Memory().Read(
            0x400000, &read, sizeof(read)) ||
        read != value) {
        std::cerr << "Machine memory read failed\n";
        return 1;
    }

    std::cout << "Machine architecture test: PASS\n";
    return 0;
}
