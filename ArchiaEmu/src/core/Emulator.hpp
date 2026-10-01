#pragma once

#include <string>

#include "cpu/Cpu.hpp"
#include "loader/Elf64Loader.hpp"
#include "memory/Memory.hpp"

namespace myps5emu {

class Emulator {
public:
    bool LoadGame(const std::string& path);
    int Run();

private:
    bool HandleSyscall(Cpu& cpu);

    Elf64Loader loader_;
    Memory memory_;
    Cpu cpu_;
    bool guest_exited_ = false;
    int guest_exit_code_ = 0;
};

} // namespace myps5emu
