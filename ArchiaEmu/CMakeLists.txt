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
    src/cpu/x86/Paging.cpp
    src/cpu/Cpu.hpp
    src/cpu/RegisterFile.cpp
    src/cpu/RegisterFile.hpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(myps5emu PRIVATE src)

add_executable(EmulatorLoadGameTest
    tools/EmulatorLoadGameTest.cpp
    src/core/Emulator.cpp
    src/core/Machine.cpp
    src/core/Bus.cpp
    src/core/RamDevice.cpp
    src/core/RomDevice.cpp
    src/core/MmioRegisterDevice.cpp
    src/loader/Elf64Loader.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(EmulatorLoadGameTest PRIVATE src)
add_test(NAME EmulatorLoadGameTest COMMAND EmulatorLoadGameTest)

add_executable(CpuAllFunctionsTest
    tools/CpuAllFunctionsTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
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
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(MachineArchitectureTest PRIVATE src)

enable_testing()
add_test(NAME CpuAllFunctionsTest COMMAND CpuAllFunctionsTest)
add_test(NAME MachineArchitectureTest COMMAND MachineArchitectureTest)

add_executable(X86ExceptionDeliveryTest
    tools/X86ExceptionDeliveryTest.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
)
target_include_directories(X86ExceptionDeliveryTest PRIVATE src)

add_test(NAME X86ExceptionDeliveryTest COMMAND X86ExceptionDeliveryTest)

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

add_executable(X86TssTest
    tools/X86TssTest.cpp
    src/cpu/x86/Tss64.cpp
)
target_include_directories(X86TssTest PRIVATE src)

add_test(NAME X86TssTest COMMAND X86TssTest)


add_executable(X86ExceptionStackFrameTest
    tools/X86ExceptionStackFrameTest.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
)
target_include_directories(X86ExceptionStackFrameTest PRIVATE src)
add_test(NAME X86ExceptionStackFrameTest COMMAND X86ExceptionStackFrameTest)


add_executable(X86ExceptionStackWriterTest
    tools/X86ExceptionStackWriterTest.cpp
    src/memory/Memory.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
)
target_include_directories(X86ExceptionStackWriterTest PRIVATE src)
add_test(NAME X86ExceptionStackWriterTest COMMAND X86ExceptionStackWriterTest)


add_executable(X86ExceptionEntryWriterTest
    tools/X86ExceptionEntryWriterTest.cpp
    src/memory/Memory.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
)
target_include_directories(X86ExceptionEntryWriterTest PRIVATE src)
add_test(NAME X86ExceptionEntryWriterTest COMMAND X86ExceptionEntryWriterTest)


add_executable(X86ExceptionStateApplierTest
    tools/X86ExceptionStateApplierTest.cpp
    src/core/Machine.cpp
    src/core/Bus.cpp
    src/core/RamDevice.cpp
    src/core/RomDevice.cpp
    src/core/MmioRegisterDevice.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86ExceptionStateApplierTest PRIVATE src)
add_test(NAME X86ExceptionStateApplierTest COMMAND X86ExceptionStateApplierTest)


add_executable(X86ExceptionEntryTest
    tools/X86ExceptionEntryTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
)
target_include_directories(X86ExceptionEntryTest PRIVATE src)
add_test(NAME X86ExceptionEntryTest COMMAND X86ExceptionEntryTest)


add_executable(X86ExceptionRoundTripTest
    tools/X86ExceptionRoundTripTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
    src/cpu/x86/ExceptionReturn64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
)
target_include_directories(X86ExceptionRoundTripTest PRIVATE src)
add_test(NAME X86ExceptionRoundTripTest COMMAND X86ExceptionRoundTripTest)

add_executable(X86SyscallCpuTest
    tools/X86SyscallCpuTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86SyscallCpuTest PRIVATE src)
add_test(NAME X86SyscallCpuTest COMMAND X86SyscallCpuTest)
add_executable(X86ExceptionCpuIntegrationTest
    tools/X86ExceptionCpuIntegrationTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86ExceptionCpuIntegrationTest PRIVATE src)
add_test(NAME X86ExceptionCpuIntegrationTest COMMAND X86ExceptionCpuIntegrationTest)


add_executable(X86PrivilegeTransitionTest
    tools/X86PrivilegeTransitionTest.cpp
    src/cpu/x86/PrivilegeTransition.cpp
)
target_include_directories(X86PrivilegeTransitionTest PRIVATE src)
add_test(NAME X86PrivilegeTransitionTest COMMAND X86PrivilegeTransitionTest)

add_executable(X86PrivilegedTest
    tools/X86PrivilegedTest.cpp
    src/cpu/x86/Privileged.cpp
)
target_include_directories(X86PrivilegedTest PRIVATE src)
add_test(NAME X86PrivilegedTest COMMAND X86PrivilegedTest)

add_executable(X86PagingCpuTest
    tools/X86PagingCpuTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86PagingCpuTest PRIVATE src)
add_test(NAME X86PagingCpuTest COMMAND X86PagingCpuTest)

add_executable(X86PagingTest
    tools/X86PagingTest.cpp
    src/memory/Memory.cpp
    src/cpu/x86/Paging.cpp
)
target_include_directories(X86PagingTest PRIVATE src)
add_test(NAME X86PagingTest COMMAND X86PagingTest)

add_executable(X86LargePageTest
    tools/X86LargePageTest.cpp
    src/memory/Memory.cpp
    src/cpu/x86/Paging.cpp
)
target_include_directories(X86LargePageTest PRIVATE src)
add_test(NAME X86LargePageTest COMMAND X86LargePageTest)

add_executable(X86ExceptionReturnTest
    tools/X86ExceptionReturnTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionReturn64.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86ExceptionReturnTest PRIVATE src)
add_test(NAME X86ExceptionReturnTest COMMAND X86ExceptionReturnTest)

add_executable(X86IretqCpuTest
    tools/X86IretqCpuTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionReturn64.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86IretqCpuTest PRIVATE src)
add_test(NAME X86IretqCpuTest COMMAND X86IretqCpuTest)

add_executable(X86InvalidOpcodeResumeTest
    tools/X86InvalidOpcodeResumeTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86InvalidOpcodeResumeTest PRIVATE src)
add_test(NAME X86InvalidOpcodeResumeTest COMMAND X86InvalidOpcodeResumeTest)

add_executable(MemoryBehaviorTest
    tools/MemoryBehaviorTest.cpp
    src/memory/Memory.cpp
)
target_include_directories(MemoryBehaviorTest PRIVATE src)
add_test(NAME MemoryBehaviorTest COMMAND MemoryBehaviorTest)

add_executable(X86SetccTest
    tools/X86SetccTest.cpp
    src/memory/Memory.cpp
    src/cpu/Cpu.cpp
    src/cpu/x86/Paging.cpp
    src/cpu/RegisterFile.cpp
    src/cpu/x86/Idt.cpp
    src/cpu/x86/ExceptionDispatcher.cpp
    src/cpu/x86/ExceptionFrame64.cpp
    src/cpu/x86/Gdt.cpp
    src/cpu/x86/ExceptionTarget.cpp
    src/cpu/x86/ExceptionDelivery.cpp
    src/cpu/x86/Tss64.cpp
    src/cpu/x86/ExceptionStackFrame64.cpp
    src/cpu/x86/ExceptionStackWriter64.cpp
    src/cpu/x86/ExceptionEntryWriter64.cpp
    src/cpu/x86/ExceptionStateApplier64.cpp
    src/cpu/x86/ExceptionEntry64.cpp
)
target_include_directories(X86SetccTest PRIVATE src)
add_test(NAME X86SetccTest COMMAND X86SetccTest)
