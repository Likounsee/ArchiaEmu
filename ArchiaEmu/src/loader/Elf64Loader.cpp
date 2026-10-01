#include "Elf64Loader.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

namespace myps5emu {

namespace {

// ELF identification
constexpr std::uint8_t ELFCLASS64 = 2;

// ELF file type / machine
constexpr std::uint16_t EM_X86_64 = 62;

// Program header type
constexpr std::uint32_t PT_LOAD = 1;

// Program header flags
constexpr std::uint32_t PF_X = 1;
constexpr std::uint32_t PF_W = 2;
constexpr std::uint32_t PF_R = 4;

#pragma pack(push, 1)

struct Elf64Header {
    std::uint8_t  ident[16];
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

bool AddOverflows(std::uint64_t a, std::uint64_t b)
{
    return b > (std::numeric_limits<std::uint64_t>::max() - a);
}

std::string FlagsToString(std::uint32_t flags)
{
    std::string result;

    result += (flags & PF_R) ? 'R' : '-';
    result += (flags & PF_W) ? 'W' : '-';
    result += (flags & PF_X) ? 'X' : '-';

    return result;
}

} // namespace

bool Elf64Loader::Load(const std::string& path)
{
    entry_point_ = 0;
    segments_.clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "[ELF] Unable to open file: " << path << '\n';
        return false;
    }

    const std::streamoff end_position = file.tellg();

    if (end_position < 0) {
        std::cerr << "[ELF] Unable to determine file size.\n";
        return false;
    }

    const std::uint64_t file_size =
        static_cast<std::uint64_t>(end_position);

    if (file_size < sizeof(Elf64Header)) {
        std::cerr << "[ELF] File is too small.\n";
        return false;
    }

    file.seekg(0, std::ios::beg);

    Elf64Header header{};

    if (!file.read(
            reinterpret_cast<char*>(&header),
            sizeof(header))) {
        std::cerr << "[ELF] Failed to read ELF header.\n";
        return false;
    }

    // ELF magic: 0x7F 'E' 'L' 'F'
    if (header.ident[0] != 0x7F ||
        header.ident[1] != 'E' ||
        header.ident[2] != 'L' ||
        header.ident[3] != 'F') {

        std::cerr << "[ELF] Invalid ELF magic.\n";
        return false;
    }

    if (header.ident[4] != ELFCLASS64) {
        std::cerr << "[ELF] File is not ELF64.\n";
        return false;
    }

    if (header.machine != EM_X86_64) {
        std::cerr << "[ELF] Unsupported machine type: "
                  << header.machine << '\n';
        return false;
    }

    if (header.program_header_entry_size !=
        sizeof(Elf64ProgramHeader)) {

        std::cerr << "[ELF] Unexpected program header size.\n";
        return false;
    }

    if (AddOverflows(
            header.program_header_offset,
            static_cast<std::uint64_t>(header.program_header_count) *
                header.program_header_entry_size)) {

        std::cerr << "[ELF] Program header table overflows.\n";
        return false;
    }

    const std::uint64_t program_headers_end =
        header.program_header_offset +
        static_cast<std::uint64_t>(header.program_header_count) *
            header.program_header_entry_size;

    if (program_headers_end > file_size) {
        std::cerr << "[ELF] Program headers exceed file size.\n";
        return false;
    }

    entry_point_ = header.entry;

    std::cout << "[ELF] Entry point: 0x"
              << std::hex << entry_point_
              << std::dec << '\n';

    for (std::uint16_t i = 0;
         i < header.program_header_count;
         ++i) {

        const std::uint64_t header_offset =
            header.program_header_offset +
            static_cast<std::uint64_t>(i) *
                header.program_header_entry_size;

        file.seekg(
            static_cast<std::streamoff>(header_offset),
            std::ios::beg);

        Elf64ProgramHeader program_header{};

        if (!file.read(
                reinterpret_cast<char*>(&program_header),
                sizeof(program_header))) {

            std::cerr << "[ELF] Failed to read program header.\n";
            return false;
        }

        if (program_header.type != PT_LOAD) {
            continue;
        }

        if (program_header.memory_size <
            program_header.file_size) {

            std::cerr << "[ELF] Invalid PT_LOAD: "
                      << "memory size < file size.\n";
            return false;
        }

        if (AddOverflows(
                program_header.offset,
                program_header.file_size)) {

            std::cerr << "[ELF] PT_LOAD file range overflows.\n";
            return false;
        }

        if (program_header.offset +
                program_header.file_size >
            file_size) {

            std::cerr << "[ELF] PT_LOAD exceeds file size.\n";
            return false;
        }

        if (program_header.memory_size >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())) {

            std::cerr << "[ELF] PT_LOAD is too large for this host.\n";
            return false;
        }

        LoadedSegment segment;

        segment.virtual_address = program_header.virtual_address;
        segment.file_size = program_header.file_size;
        segment.memory_size = program_header.memory_size;
        segment.flags = program_header.flags;

        segment.data.resize(
            static_cast<std::size_t>(
                program_header.memory_size),
            0);

        if (program_header.file_size != 0) {

            file.seekg(
                static_cast<std::streamoff>(
                    program_header.offset),
                std::ios::beg);

            if (!file.read(
                    reinterpret_cast<char*>(segment.data.data()),
                    static_cast<std::streamsize>(
                        program_header.file_size))) {

                std::cerr << "[ELF] Failed to read PT_LOAD data.\n";
                return false;
            }
        }

        std::cout
            << "[ELF] PT_LOAD"
            << " VA=0x" << std::hex
            << segment.virtual_address
            << " FileSize=0x"
            << segment.file_size
            << " MemSize=0x"
            << segment.memory_size
            << " Flags="
            << FlagsToString(segment.flags)
            << std::dec
            << '\n';

        segments_.push_back(std::move(segment));
    }

    if (segments_.empty()) {
        std::cerr << "[ELF] No PT_LOAD segment found.\n";
        return false;
    }

    std::cout
        << "[ELF] Loaded "
        << segments_.size()
        << " PT_LOAD segment(s).\n";

    return true;
}

} // namespace myps5emu