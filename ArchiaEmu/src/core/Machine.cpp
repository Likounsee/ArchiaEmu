#include "Machine.hpp"

namespace myps5emu {

Machine::Machine()
    : paging_(bus_)
{
    cpu_.ConnectMemory(&bus_);
    cpu_.SetPaging(&paging_);
}

Cpu& Machine::CPU() noexcept
{
    return cpu_;
}

const Cpu& Machine::CPU() const noexcept
{
    return cpu_;
}

Memory& Machine::GuestMemory() noexcept
{
    return bus_;
}

const Memory& Machine::GuestMemory() const noexcept
{
    return bus_;
}

Bus& Machine::SystemBus() noexcept
{
    return bus_;
}

const Bus& Machine::SystemBus() const noexcept
{
    return bus_;
}

} // namespace myps5emu
