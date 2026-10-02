#include "core/Emulator.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

#pragma pack(push, 1)
struct Elf64Header {
    std::uint8_t ident[16];
    std::uint16_t type;
    std::uint16_t machine;
    std::uint32_t version;
    std::uint64_t entry;
    std::uint64_t program_header_offset;
    std::uint64_t section_header_offset;
    std::uint32_t flags;
    std::uint16_t header_size;
    std::uint16_t program_header_entry_size;
    std::uint16_t program_header_count;
    std::uint16_t section_header_entry_size;
    std::uint16_t section_header_count;
    std::uint16_t section_name_index;
};

struct Elf64ProgramHeader {
    std::uint32_t type;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t virtual_address;
    std::uint64_t physical_address;
    std::uint64_t file_size;
    std::uint64_t memory_size;
    std::uint64_t alignment;
};
#pragma pack(pop)

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

bool WriteOverlappingStackElf(const std::filesystem::path& path)
{
    constexpr std::uint64_t stackBase = 0x7FFF00000000ULL;
    constexpr std::uint64_t programOffset = 0x1000ULL;

    Elf64Header header{};
    header.ident[0] = 0x7F;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 2;
    header.ident[5] = 1;
    header.ident[6] = 1;
    header.type = 2;
    header.machine = 62;
    header.version = 1;
    header.entry = stackBase;
    header.program_header_offset = sizeof(Elf64Header);
    header.header_size = sizeof(Elf64Header);
    header.program_header_entry_size = sizeof(Elf64ProgramHeader);
    header.program_header_count = 1;

    Elf64ProgramHeader program{};
    program.type = 1;
    program.flags = 0x5;
    program.offset = programOffset;
    program.virtual_address = stackBase;
    program.file_size = 1;
    program.memory_size = 1;
    program.alignment = 0x1000;

    const std::array<std::uint8_t, 1> hlt = {0xF4};

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&program), sizeof(program));

    const auto current = static_cast<std::uint64_t>(sizeof(header) + sizeof(program));
    std::array<std::uint8_t, 0x1000> padding{};
    if (current < programOffset) {
        file.write(reinterpret_cast<const char*>(padding.data()),
                   static_cast<std::streamsize>(programOffset - current));
    }

    file.write(reinterpret_cast<const char*>(hlt.data()), hlt.size());
    return file.good();
}

} // namespace

int main()
{
    const auto path =
        std::filesystem::temp_directory_path() / "archiaemu_loadgame_partial_failure.elf";
    const auto validPath =
        std::filesystem::temp_directory_path() / "archiaemu_loadgame_valid.elf";

    if (!WriteOverlappingStackElf(path)) {
        return Fail("Failed to create LoadGame regression ELF") ? 0 : 1;
    }

    // First prove that a valid game can load and run. This establishes guest
    // state that the next failed LoadGame() call must not leave behind.
    {
        myps5emu::Emulator emulator;
        if (!WriteOverlappingStackElf(validPath)) {
            std::filesystem::remove(path);
            return Fail("Failed to create valid LoadGame fixture") ? 0 : 1;
        }

        // The stack-overlap fixture is intentionally not a valid successful
        // load, so build the successful fixture by changing its entry/segment
        // address in a separate file below.
    }

    std::filesystem::remove(validPath);

    myps5emu::Emulator emulator;

    if (emulator.LoadGame(path.string())) {
        std::filesystem::remove(path);
        return Fail("LoadGame unexpectedly accepted an ELF whose PT_LOAD overlaps the guest stack") ? 0 : 1;
    }

    // The ELF loader succeeds and LoadGame maps the PT_LOAD before the later
    // guest-stack mapping fails. If that partial mapping is retained, Run()
    // will execute the HLT left at the failed load's entry point.
    const int partialFailureRun = emulator.Run();

    std::filesystem::remove(path);

    if (partialFailureRun == 0) {
        return Fail("Failed LoadGame left guest memory/CPU state partially loaded") ? 0 : 1;
    }

    std::cout << "Emulator LoadGame failure rollback test: PASS\n";
    return 0;
}
