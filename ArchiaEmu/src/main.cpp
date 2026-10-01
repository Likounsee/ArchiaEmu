#include "core/Emulator.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: myps5emu <elf-file>\n";
        return 1;
    }

    myps5emu::Emulator emulator;

    if (!emulator.LoadGame(argv[1])) {
        std::cerr << "Failed to load ELF.\n";
        return 1;
    }

    return emulator.Run();
}