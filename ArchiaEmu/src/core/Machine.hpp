#pragma once

#include "core/Bus.hpp"
#include "cpu/Cpu.hpp"
#include "cpu/x86/Paging.hpp"
#include "storage/OpenFs.hpp"

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

    storage::OpenFs& Storage() noexcept;
    const storage::OpenFs& Storage() const noexcept;

private:
    Bus bus_;
    Paging paging_;
    Cpu cpu_;
    storage::OpenFs storage_;
};

} // namespace myps5emu
