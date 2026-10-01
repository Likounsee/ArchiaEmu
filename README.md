# ArchiaEmu

ArchiaEmu is an experimental C++20 **general-purpose emulation project**.

The long-term goal is to build a reusable emulation framework that can host different CPU architectures, machines, operating systems and hardware devices. Specific consoles and platforms are targets built on top of that framework; they are not assumed to be compatible merely because a shared CPU architecture is supported.

> **Current status:** early research and development. ArchiaEmu is not a complete console emulator.

## Current foundation

The repository currently contains a tested x86-64 guest execution foundation:

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
- Dedicated CPU regression/function tests
- A first `Machine` runtime boundary owning CPU and guest memory

The CPU implementation and tests are developed incrementally with an emphasis on architectural correctness and regression coverage.

## Architecture direction

The project is being evolved toward a layered, reusable design:

```text
ArchiaEmu
├── Core
│   ├── Emulator
│   └── Machine
├── CPU
│   └── x86-64 (current implementation)
├── Memory
├── Loaders
│   └── ELF64 (current implementation)
├── Bus / Devices (future)
├── Operating-system interfaces (future)
└── Platforms / Machines (future)
    ├── PC
    ├── PlayStation
    ├── Xbox
    ├── Nintendo
    └── other systems
```

The important separation is between **CPU architecture** and **machine/platform**. For example, two machines may use related CPU technology while having completely different memory maps, devices, firmware and operating systems.

The current `Machine` layer owns the concrete guest CPU and memory. This is an incremental architectural boundary; it does not claim that other CPU architectures or consoles are already implemented.

## Build

ArchiaEmu uses **CMake 3.20+** and **C++20**.

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

## Tests

The project currently contains a dedicated CPU regression/function test executable plus a machine architecture test.

The CPU test source currently contains **56 test functions and 172 `CHECK(...)` assertions**.

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
- instruction-byte consumption order regressions

CMake/CTest is the source of truth for the runnable test suite.

## Running the current emulator

The current executable accepts an ELF64 file:

```powershell
.\ArchiaEmu\build\myps5emu.exe path\to\program.elf
```

At the current stage, the loader accepts ELF64 x86-64 executables and maps their loadable segments into guest memory.

This does **not** mean that arbitrary console executables or arbitrary x86-64 software are currently supported.

## Development strategy

ArchiaEmu is being developed in layers:

1. Establish a correct and testable CPU foundation.
2. Keep guest memory and executable loading independently testable.
3. Introduce reusable machine/core boundaries without changing verified CPU behavior.
4. Add a bus and device model when the first machine targets require them.
5. Add additional CPU architectures only when the common interfaces are stable enough to support them cleanly.
6. Build real platform models from documented hardware/software behavior.
7. Add optimization such as JIT/recompilation only after correctness and regression coverage are strong.

**Correctness and reproducible tests take priority over performance and premature abstraction.**

## Roadmap

Planned areas include:

- broader x86-64 instruction coverage and architectural audits
- more complete memory and virtual-memory semantics
- bus and device abstractions
- syscall and operating-system interfaces
- threading and synchronization
- additional CPU architectures such as AArch64, MIPS and PowerPC
- loaders for additional executable/ROM formats
- GPU, audio, input and storage devices
- debugging and compatibility tooling
- concrete PC and console platform models
- PS5-specific research as one platform target among others
- JIT/recompiler work after the interpreter foundation is sufficiently verified

None of these future items should be interpreted as existing compatibility.

## Project status

ArchiaEmu is an educational/research project under active development.

The repository is the source of truth for what is actually implemented and tested.

Features are considered complete only after they have been implemented and positively verified by tests.

<!-- Temporary memory CI validation -->
