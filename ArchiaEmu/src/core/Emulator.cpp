#include "Emulator.hpp"

#include <iostream>
#include <vector>

namespace myps5emu {

bool Emulator::LoadGame(
    const std::string& path)
{
    guest_exited_ = false;
    guest_exit_code_ = 0;

    if (!loader_.Load(path)) {

        std::cerr
            << "[Emulator] Failed to load ELF.\n";

        return false;
    }

    for (const auto& segment :
         loader_.Segments()) {

        if (!memory_.Map(
                segment.virtual_address,
                static_cast<std::size_t>(
                    segment.memory_size))) {

            std::cerr
                << "[Memory] Failed to map segment at 0x"
                << std::hex
                << segment.virtual_address
                << std::dec
                << '\n';

            return false;
        }

        if (!segment.data.empty() &&
            !memory_.Write(
                segment.virtual_address,
                segment.data.data(),
                segment.data.size())) {

            std::cerr
                << "[Memory] Failed to load segment at 0x"
                << std::hex
                << segment.virtual_address
                << std::dec
                << '\n';

            return false;
        }

        std::cout
            << "[Memory] Loaded PT_LOAD at 0x"
            << std::hex
            << segment.virtual_address
            << " ("
            << std::dec
            << segment.memory_size
            << " bytes)"
            << '\n';
    }

    constexpr std::uint64_t stackBase =
        0x7FFF00000000ULL;

    constexpr std::size_t stackSize =
        0x10000;

    if (!memory_.Map(
            stackBase,
            stackSize)) {

        std::cerr
            << "[Memory] Failed to map guest stack.\n";

        return false;
    }

    cpu_.ConnectMemory(
        &memory_);
    cpu_.SetSyscallHandler(
        [this](Cpu& cpu) {
            return HandleSyscall(cpu);
        });

    cpu_.SetStackPointer(
        stackBase + stackSize);

    std::cout
        << "[Emulator] Entry point: 0x"
        << std::hex
        << loader_.EntryPoint()
        << std::dec
        << '\n';

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
        !memory_.Read(buffer_address, buffer.data(), buffer.size())) {
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
    cpu_.SetInstructionPointer(
        loader_.EntryPoint());

    std::cout
        << "[CPU] Starting at 0x"
        << std::hex
        << cpu_.InstructionPointer()
        << std::dec
        << '\n';

    const int result = cpu_.Run();

    if (result != 0) {
        return result;
    }

    return guest_exited_ ? guest_exit_code_ : 0;
}

} // namespace myps5emu
