cmake_minimum_required(VERSION 3.20)

project(ArchiaEmu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(myps5emu
    src/main.cpp
    src/core/Emulator.cpp
    src/core/Emulator.hpp
    src/core/Machine.cpp
    src/core/Machine.hpp
    src/core/Bus.cpp
    src/core/Bus.hpp
    src/core/Device.hpp
    src/core/RamDevice.cpp
    src/core/RamDevice.hpp
    src/core/RomDevice.cpp
    src/core/RomDevice.hpp
    src/core/MmioRegisterDevice.cpp
    src/core/MmioRegisterDevice.hpp
    src/loader/Elf64Loader.cpp
    src/loader/Elf64Loader.hpp
    src/memory/Memory.cpp
    src/memory/Memory.hpp
    src/cpu/Cpu.cpp
    src/cpu/Cpu.hpp
    src/cpu/RegisterFile.cpp
    src/cpu/RegisterFile.hpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
)
target_include_directories(myps5emu PRIVATE src)

add_executable(CpuAllFunctionsTest
    tools/CpuAllFunctionsTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/RegisterFile.cpp
)
target_include_directories(CpuAllFunctionsTest PRIVATE src)

add_executable(MachineArchitectureTest
    tools/MachineArchitectureTest.cpp
    src/core/Machine.cpp
    src/core/Bus.cpp
    src/core/RamDevice.cpp
    src/core/RomDevice.cpp
    src/core/MmioRegisterDevice.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/RegisterFile.cpp
)
target_include_directories(MachineArchitectureTest PRIVATE src)

enable_testing()
add_test(NAME CpuAllFunctionsTest COMMAND CpuAllFunctionsTest)
add_test(NAME MachineArchitectureTest COMMAND MachineArchitectureTest)

add_executable(X86GdtTest
    tools/X86GdtTest.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
)
target_include_directories(X86GdtTest PRIVATE src)

add_test(NAME X86GdtTest COMMAND X86GdtTest)

add_executable(X86ExceptionFrameTest
    tools/X86ExceptionFrameTest.cpp
    src/cpu/x86/ExceptionFrame64.cpp
)
target_include_directories(X86ExceptionFrameTest PRIVATE src)

add_test(NAME X86ExceptionFrameTest COMMAND X86ExceptionFrameTest)

add_executable(X86IdtTest
    tools/X86IdtTest.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
)
target_include_directories(X86IdtTest PRIVATE src)

add_test(NAME X86IdtTest COMMAND X86IdtTest)
