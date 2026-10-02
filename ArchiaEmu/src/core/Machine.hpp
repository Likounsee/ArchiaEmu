#pragma once

#include "core/Bus.hpp"
#include "cpu/Cpu.hpp"
#include "cpu/x86/Paging.hpp"

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
    Paging paging_;
    Cpu cpu_;
};

} // namespace myps5emu
