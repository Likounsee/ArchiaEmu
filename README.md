# ArchiaEmu

ArchiaEmu is an experimental C++20 emulator project initially focused on the **PlayStation 5**.

The long-term goal is to build a clean and reusable emulation architecture that can evolve beyond a single console. Support for additional consoles is a future direction, not a claim of current compatibility.

> **Current status:** early research and development. ArchiaEmu is **not a working PS5 emulator yet**.

## Current project

The repository currently contains the foundations of an x86-64 guest execution environment:

- x86-64 CPU state and instruction execution
- General-purpose register file
- REX prefix handling
- ModRM/SIB addressing
- 8/32/64-bit integer operations
- Memory operations
- Stack and control-flow instructions
- Integer arithmetic, logic, shifts and rotates
- Basic flag handling
- ELF64 validation and PT_LOAD loading
- Guest memory mapping
- Basic Linux-style syscall handling for `write` and `exit`
- A dedicated CPU regression/function test program

The CPU implementation and tests are being developed incrementally with an emphasis on architectural correctness and regression coverage.

## Architecture

The current source tree is organized around several responsibilities:

```text
ArchiaEmu/
├── src/
│   ├── core/       Emulator orchestration
│   ├── cpu/        x86-64 CPU and register state
│   ├── loader/     ELF64 loading
│   ├── memory/     Guest memory management
│   └── main.cpp    Program entry point
├── tools/
│   ├── CpuAllFunctionsTest.cpp
│   └── build_cpu_test.bat
└── CMakeLists.txt
```

The separation is intentional: CPU execution, guest memory, executable loading and emulator orchestration should remain independently testable as the project grows.

## Build

ArchiaEmu currently uses **CMake 3.20+** and **C++20**.

From the repository root on Windows:

```powershell
cmake -S ArchiaEmu -B ArchiaEmu/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ArchiaEmu/build
```

A Visual Studio generator can also be used:

```powershell
cmake -S ArchiaEmu -B ArchiaEmu/build
cmake --build ArchiaEmu/build --config Debug
```

## CPU tests

The project includes a dedicated CPU test executable covering a large set of instruction and regression cases.

The current test source contains **56 test functions and 172 `CHECK(...)` assertions**.

Coverage includes:

- arithmetic and logical operations
- comparisons and flags
- memory operands
- ModRM/SIB addressing
- REX and extended registers
- 8-bit register edge cases
- stack operations
- branches and calls/returns
- shifts and rotates
- sign/zero extension
- `TEST`
- `XCHG`
- `IMUL`
- `DIV`
- `IDIV`
- syscall dispatch
- regression tests for instruction-byte consumption order

After building, run the generated `CpuAllFunctionsTest` executable and verify its final PASS/FAIL/TOTAL result.

The test program is intentionally kept explicit and close to the instruction semantics so that CPU regressions can be reproduced and diagnosed.

## Running the emulator

The current executable accepts an ELF64 file:

```powershell
.\ArchiaEmu\build\myps5emu.exe path\to\program.elf
```

At the current stage, the loader accepts ELF64 x86-64 executables and maps their loadable segments into guest memory.

This does **not** mean that arbitrary PS5 executables are currently supported.

## Development philosophy

ArchiaEmu is being developed incrementally:

1. Establish a correct and testable CPU foundation.
2. Implement guest memory and executable loading correctly.
3. Add instruction coverage with regression tests.
4. Build the system and execution layers around the verified CPU core.
5. Progressively investigate PS5-specific operating-system, hardware and runtime requirements.
6. Keep the architecture reusable enough to support other console targets where their hardware and software models make that practical.

**Correctness and reproducible tests take priority over performance and premature abstraction.**

## Roadmap

The roadmap is intentionally progressive. Planned areas include:

- broader x86-64 instruction coverage
- more complete ELF64 loading and memory semantics
- virtual-memory and protection semantics
- syscall and operating-system interfaces
- threading and synchronization primitives
- JIT/recompiler work
- GPU command processing
- graphics API translation
- input, audio and filesystem services
- debugging and compatibility tooling
- PS5-specific platform work
- investigation of reusable platform abstractions for additional consoles

None of these future items should be interpreted as existing compatibility.

## Project status

ArchiaEmu is an educational/research project under active development.

The repository should always be treated as the source of truth for what is actually implemented and tested.

Features are considered complete only after they have been implemented and positively verified by tests.
