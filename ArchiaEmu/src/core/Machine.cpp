#include "Machine.hpp"
#include "core/Bus.hpp"

namespace myps5emu {

Machine::Machine()
{
    cpu_.ConnectMemory(&bus_);
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

} // namespace myps5emu
