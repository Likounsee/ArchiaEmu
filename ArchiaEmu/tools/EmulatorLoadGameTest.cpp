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

bool WriteElf(const std::filesystem::path& path, std::uint64_t virtualAddress)
{
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
    header.entry = virtualAddress;
    header.program_header_offset = sizeof(Elf64Header);
    header.header_size = sizeof(Elf64Header);
    header.program_header_entry_size = sizeof(Elf64ProgramHeader);
    header.program_header_count = 1;

    Elf64ProgramHeader program{};
    program.type = 1;
    program.flags = 0x5;
    program.offset = programOffset;
    program.virtual_address = virtualAddress;
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
    constexpr std::uint64_t stackBase = 0x7FFF00000000ULL;
    constexpr std::uint64_t validBase = 0x400000ULL;

    const auto partialPath =
        std::filesystem::temp_directory_path() / "archiaemu_loadgame_partial_failure.elf";
    const auto validPath =
        std::filesystem::temp_directory_path() / "archiaemu_loadgame_valid.elf";

    if (!WriteElf(partialPath, stackBase) ||
        !WriteElf(validPath, validBase)) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("Failed to create LoadGame regression ELF fixtures") ? 0 : 1;
    }

    myps5emu::Emulator emulator;

    // Baseline: a normal ELF loads and its HLT can run.
    if (!emulator.LoadGame(validPath.string()) || emulator.Run() != 0) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("Valid LoadGame baseline failed") ? 0 : 1;
    }

    // Repeated LoadGame() with an invalid path must not leave the previous
    // game's memory/entry point runnable.
    if (emulator.LoadGame(validPath.string() + ".missing")) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("LoadGame unexpectedly accepted a missing ELF") ? 0 : 1;
    }

    if (emulator.Run() == 0) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("Failed repeated LoadGame left the previous game loaded") ? 0 : 1;
    }

    // The ELF loader succeeds here, but LoadGame maps the PT_LOAD before the
    // later guest-stack mapping fails because both use stackBase. The failed
    // load must not leave that PT_LOAD executable/runnable.
    if (emulator.LoadGame(partialPath.string())) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("LoadGame unexpectedly accepted an ELF whose PT_LOAD overlaps the guest stack") ? 0 : 1;
    }

    if (emulator.Run() == 0) {
        std::filesystem::remove(partialPath);
        std::filesystem::remove(validPath);
        return Fail("Failed LoadGame left guest memory/CPU state partially loaded") ? 0 : 1;
    }

    std::filesystem::remove(partialPath);
    std::filesystem::remove(validPath);

    std::cout << "Emulator LoadGame failure rollback test: PASS\n";
    return 0;
}
