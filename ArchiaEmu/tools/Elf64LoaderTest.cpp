#include "loader/Elf64Loader.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

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

int main()
{
    const auto path =
        std::filesystem::temp_directory_path() / "archiaemu_loader_sparse_bss.elf";

    constexpr std::uint64_t fileOffset = 0x1000;
    constexpr std::uint64_t virtualAddress = 0x400000;
    constexpr std::uint64_t fileSize = 1;
    constexpr std::uint64_t memorySize = 4ULL * 1024ULL * 1024ULL * 1024ULL;

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
    header.program_header_offset = sizeof(header);
    header.header_size = sizeof(header);
    header.program_header_entry_size = sizeof(Elf64ProgramHeader);
    header.program_header_count = 1;

    Elf64ProgramHeader program{};
    program.type = 1;
    program.flags = 5;
    program.offset = fileOffset;
    program.virtual_address = virtualAddress;
    program.file_size = fileSize;
    program.memory_size = memorySize;
    program.alignment = 0x1000;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return 1;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&program), sizeof(program));

    std::array<std::uint8_t, 0x1000> padding{};
    const auto current =
        static_cast<std::uint64_t>(sizeof(header) + sizeof(program));
    if (current < fileOffset) {
        file.write(reinterpret_cast<const char*>(padding.data()),
                   static_cast<std::streamsize>(fileOffset - current));
    }
    const std::uint8_t byte = 0xF4;
    file.write(reinterpret_cast<const char*>(&byte), 1);
    file.close();

    myps5emu::Elf64Loader loader;
    const bool loaded = loader.Load(path.string());

    std::filesystem::remove(path);

    if (!loaded || loader.Segments().size() != 1) return 2;
    const auto& segment = loader.Segments().front();
    if (segment.file_size != fileSize ||
        segment.memory_size != memorySize ||
        segment.data.size() != fileSize ||
        segment.data[0] != byte) {
        std::cerr << "ELF loader materialized BSS in host memory\n";
        return 3;
    }

    std::cout << "ELF loader sparse BSS test: PASS\n";
    return 0;
}
