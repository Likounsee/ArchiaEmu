#pragma once

#include "cpu/Cpu.hpp"
#include "memory/Memory.hpp"

namespace myps5emu {

/**
 * Runtime state for an emulated machine.
 *
 * A machine owns the guest CPU and guest memory so that platform-specific
 * orchestration can be built around a stable hardware boundary. The concrete
 * CPU and memory implementations remain replaceable as the emulator grows.
 */
class Machine {
public:
    Machine();

    Cpu& CPU() noexcept;
    const Cpu& CPU() const noexcept;

    Memory& GuestMemory() noexcept;
    const Memory& GuestMemory() const noexcept;

private:
    myps5emu::Memory memory_;
    myps5emu::Cpu cpu_;
};

} // namespace myps5emu
