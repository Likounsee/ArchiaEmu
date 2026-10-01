#pragma once

#include "core/Bus.hpp"
#include "cpu/Cpu.hpp"

namespace myps5emu {

class Machine {
public:
    Machine();

    Cpu& CPU() noexcept;
    const Cpu& CPU() const noexcept;

    Memory& GuestMemory() noexcept;
    const Memory& GuestMemory() const noexcept;

    Bus& SystemBus() noexcept;
    const Bus& SystemBus() const noexcept;

private:
    Bus bus_;
    Cpu cpu_;
};

} // namespace myps5emu
