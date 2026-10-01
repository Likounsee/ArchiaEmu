#include "Machine.hpp"

namespace myps5emu {

Machine::Machine()
{
    cpu_.ConnectMemory(&memory_);
}

Cpu& Machine::CPU() noexcept
{
    return cpu_;
}

const Cpu& Machine::CPU() const noexcept
{
    return cpu_;
}

Memory& Machine::Memory() noexcept
{
    return memory_;
}

const Memory& Machine::Memory() const noexcept
{
    return memory_;
}

} // namespace myps5emu
