#include "Emulator.hpp"

#include <iostream>
#include <limits>
#include <vector>

namespace myps5emu {

bool Emulator::LoadGame(const std::string& path)
{
    guest_exited_ = false;
    guest_exit_code_ = 0;

    if (!loader_.Load(path)) {
        std::cerr << "[Emulator] Failed to load ELF.\n";
        return false;
    }

    auto& memory = machine_.GuestMemory();
    auto& cpu = machine_.CPU();

    constexpr std::uint32_t elf_pf_r = 0x4U;
    constexpr std::uint32_t elf_pf_w = 0x2U;
    constexpr std::uint32_t elf_pf_x = 0x1U;

    for (const auto& segment : loader_.Segments()) {
        if (segment.memory_size == 0 ||
            segment.virtual_address >
                std::numeric_limits<std::uint64_t>::max() -
                    segment.memory_size) {
            std::cerr << "[Memory] Invalid ELF segment range.\n";
            return false;
        }

        const std::uint64_t segment_end =
            segment.virtual_address + segment.memory_size;
        const std::uint64_t map_base =
            segment.virtual_address -
            (segment.virtual_address % Memory::PageSize);
        if (segment_end >
            std::numeric_limits<std::uint64_t>::max() -
                (Memory::PageSize - 1)) {
            std::cerr << "[Memory] ELF segment alignment overflow.\n";
            return false;
        }
        const std::uint64_t map_end =
            (segment_end + Memory::PageSize - 1) /
            Memory::PageSize * Memory::PageSize;
        const std::uint64_t map_size64 = map_end - map_base;
        if (map_size64 > std::numeric_limits<std::size_t>::max()) {
            std::cerr << "[Memory] ELF mapping is too large.\n";
            return false;
        }
        const std::size_t map_size =
            static_cast<std::size_t>(map_size64);

        MemoryPermission permissions = MemoryPermission::None;
        if ((segment.flags & elf_pf_r) != 0) {
            permissions = permissions | MemoryPermission::Read;
        }
        if ((segment.flags & elf_pf_w) != 0) {
            permissions = permissions | MemoryPermission::Write;
        }
        if ((segment.flags & elf_pf_x) != 0) {
            permissions = permissions | MemoryPermission::Execute;
        }

        if (!memory.Map(map_base, map_size, permissions)) {
            std::cerr
                << "[Memory] Failed to map segment at 0x"
                << std::hex << segment.virtual_address
                << std::dec << '\n';
            return false;
        }

        if (!segment.data.empty() &&
            !memory.Write(
                segment.virtual_address,
                segment.data.data(),
                segment.data.size())) {
            std::cerr
                << "[Memory] Failed to load segment at 0x"
                << std::hex << segment.virtual_address
                << std::dec << '\n';
            return false;
        }

        std::cout
            << "[Memory] Loaded PT_LOAD at 0x"
            << std::hex << segment.virtual_address
            << " (" << std::dec << segment.memory_size
            << " bytes)\n";
    }

    constexpr std::uint64_t stackBase = 0x7FFF00000000ULL;
    constexpr std::size_t stackSize = 0x10000;

    if (!memory.Map(stackBase, stackSize,
                    MemoryPermission::Read | MemoryPermission::Write)) {
        std::cerr << "[Memory] Failed to map guest stack.\n";
        return false;
    }

    cpu.SetSyscallHandler(
        [this](Cpu& syscallCpu) {
            return HandleSyscall(syscallCpu);
        });

    cpu.SetStackPointer(stackBase + stackSize);

    std::cout
        << "[Emulator] Entry point: 0x"
        << std::hex << loader_.EntryPoint()
        << std::dec << '\n';

    return true;
}

bool Emulator::HandleSyscall(Cpu& cpu)
{
    constexpr std::uint64_t write_syscall = 1;
    constexpr std::uint64_t exit_syscall = 60;
    constexpr std::size_t max_write_size = 1024 * 1024;

    const std::uint64_t syscall_number = cpu.ReadRegister64(0);

    if (syscall_number == exit_syscall) {
        guest_exit_code_ = static_cast<int>(
            cpu.ReadRegister64(7) & 0xFF);
        guest_exited_ = true;
        cpu.Halt();
        return true;
    }

    if (syscall_number != write_syscall) {
        std::cerr << "[Syscall] Unsupported syscall: "
                  << syscall_number << '\n';
        return false;
    }

    const std::uint64_t file_descriptor = cpu.ReadRegister64(7);
    const std::uint64_t buffer_address = cpu.ReadRegister64(6);
    const std::uint64_t byte_count = cpu.ReadRegister64(2);

    if ((file_descriptor != 1 && file_descriptor != 2) ||
        byte_count > max_write_size) {
        std::cerr << "[Syscall] Invalid write request.\n";
        return false;
    }

    std::vector<std::uint8_t> buffer(
        static_cast<std::size_t>(byte_count));

    if (byte_count != 0 &&
        !machine_.GuestMemory().Read(
            buffer_address, buffer.data(), buffer.size())) {
        std::cerr << "[Syscall] Write buffer is not mapped.\n";
        return false;
    }

    std::ostream& output =
        (file_descriptor == 1) ? std::cout : std::cerr;
    output.write(
        reinterpret_cast<const char*>(buffer.data()),
        static_cast<std::streamsize>(buffer.size()));
    output.flush();

    cpu.WriteRegister64(0, byte_count);
    return true;
}

int Emulator::Run()
{
    auto& cpu = machine_.CPU();

    cpu.SetInstructionPointer(loader_.EntryPoint());

    std::cout
        << "[CPU] Starting at 0x"
        << std::hex << cpu.InstructionPointer()
        << std::dec << '\n';

    const int result = cpu.Run();

    if (result != 0) {
        return result;
    }

    return guest_exited_ ? guest_exit_code_ : 0;
}

} // namespace myps5emu
