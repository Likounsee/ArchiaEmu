# ArchiaEmu

ArchiaEmu is an experimental C++20 **general-purpose emulation framework**.

The long-term goal is to build a reusable emulator capable of supporting a very large range of machines and consoles, from older systems to modern platforms. Consoles are platform implementations built on top of reusable CPU, memory, bus and device components.

> **Current status:** active research and development. ArchiaEmu is **not yet a complete console emulator**.

## Project goal

The target is not a single console.

ArchiaEmu is being designed so that the same core can eventually host many different platforms:

- PlayStation families
- Xbox families
- Nintendo systems
- PC-compatible machines
- handhelds
- arcade and other specialized systems
- additional platforms when their documented hardware/software behavior can be modeled

Supporting a CPU architecture does **not** imply compatibility with every machine using that architecture. Each platform needs its own CPU configuration, memory map, firmware, devices, buses, operating-system interfaces and other hardware behavior.

The priority is therefore to build a strong reusable foundation first, then use it to implement concrete machines one at a time.

## Current architecture

The repository currently has these major layers:

```text
ArchiaEmu
├── Core
│   ├── Emulator
│   ├── Machine
│   ├── Bus
│   └── Devices
├── CPU
│   └── x86-64 (current implementation)
├── Memory
├── Loaders
│   └── ELF64 (current implementation)
├── x86 architecture support
│   ├── Exceptions
│   ├── GDT / IDT / TSS
│   ├── Paging
│   └── Privilege/return paths
├── Operating-system interfaces
│   └── basic syscall foundation
└── Platforms / Machines
    └── future console and computer implementations
```

The important separation is between **CPU architecture** and **machine/platform**. The CPU is reusable; a machine defines how that CPU interacts with memory, buses, devices, firmware and software.

## Current implementation status

### CPU: x86-64 foundation — IN PROGRESS

A substantial x86-64 interpreter foundation is implemented and under regression testing.

Implemented/tested areas include:

- general-purpose register state
- instruction fetch/decode/execute
- REX prefixes and extended registers
- ModRM and SIB addressing
- operand-size overrides
- address-size override support, including dedicated regression coverage
- 8/16/32/64-bit integer operations
- register and memory operands
- immediate operands
- arithmetic and logical operations
- comparisons and flag handling
- ADC/SBB
- IMUL
- DIV/IDIV
- shifts and rotates
- TEST
- XCHG
- LEA
- MOV variants
- INC/DEC/NEG
- stack operations
- PUSH/POP and immediate stack forms
- CALL/RET
- conditional branches
- SETcc
- HLT
- syscall dispatch and fallback behavior
- invalid-opcode/exception-related execution paths

The CPU implementation is **not a complete x86-64 ISA implementation**. Unsupported instructions and architectural corner cases remain part of the ongoing audit.

### x86 architectural support — FOUNDATION IMPLEMENTED, AUDIT IN PROGRESS

The repository contains dedicated components and tests for:

- exception delivery
- exception entry
- exception stack frames
- exception return paths
- GDT
- IDT
- TSS64
- privilege transitions
- privileged instruction handling
- paging
- large-page handling
- CPU/paging integration
- IRETQ integration
- canonical-address checks

These components provide the foundation required for protected/privileged execution, but they are not yet considered a complete implementation of all x86-64 architectural behavior.

### Memory — FOUNDATION IMPLEMENTED

The project has a dedicated guest-memory subsystem with:

- mapped guest memory
- read/write operations
- access/error handling
- integration with CPU execution
- dedicated memory behavior tests

Virtual-memory/paging support is also present in the x86 layer and is being expanded and audited independently from the basic memory subsystem.

### Core / Machine / Bus — FOUNDATION IMPLEMENTED

The reusable runtime boundary currently contains:

- `Emulator`
- `Machine`
- `Bus`
- `Device`
- RAM device
- ROM device
- MMIO register device

Dedicated architecture tests verify the machine/bus/device boundaries.

This is the beginning of the reusable multi-platform architecture. It is **not yet a collection of complete console machines**.

### ELF64 loading — IMPLEMENTED FOUNDATION

The current executable loader supports validation and loading of ELF64 x86-64 executables, including loadable segments and guest-memory mapping.

Dedicated loader tests and emulator loading tests are present.

This does **not** mean that arbitrary console executables are currently supported.

## Test status

CMake/CTest is the source of truth for the runnable test suite.

The current branch contains **30 registered CTest targets**, covering:

- CPU function/regression testing
- x86 exceptions and exception return
- GDT / IDT / TSS
- privilege transitions
- privileged CPU paths
- paging and large pages
- address-size override
- IRETQ
- SETcc
- syscall execution
- memory behavior
- bus behavior
- machine architecture
- ELF64 loading
- emulator game/loading paths

The main `CpuAllFunctionsTest` currently contains **58 CPU test functions and 218 `CHECK(...)` assertions**. Additional dedicated executables provide focused architectural regression coverage.

Tests are deliberately kept separate and reproducible so that fixing one architectural area does not silently weaken another.

## Development status

### Completed foundations

- [x] C++20/CMake project foundation
- [x] x86-64 register state
- [x] core instruction execution framework
- [x] REX handling
- [x] ModRM/SIB addressing
- [x] operand-size handling
- [x] broad integer arithmetic/logic foundation
- [x] stack and control-flow foundation
- [x] basic flag handling
- [x] guest memory subsystem
- [x] ELF64 loading foundation
- [x] reusable Machine boundary
- [x] Bus/Device foundation
- [x] RAM/ROM/MMIO device foundation
- [x] syscall foundation
- [x] exception/paging architectural foundation
- [x] dedicated regression test infrastructure
- [x] GitHub Actions build/test workflows

### In progress

- [ ] finish the x86-64 architectural audit
- [ ] finish all important 32-bit address-size override edge cases
- [ ] expand privileged/control-instruction coverage
- [ ] expand virtual-memory and paging semantics
- [ ] expand instruction decoding and architectural corner cases
- [ ] increase integration coverage between CPU, memory, paging, bus and devices
- [ ] keep the complete CTest suite continuously green
- [ ] improve documentation of architectural guarantees and unsupported behavior

### Not started as complete platform implementations

- [ ] PlayStation machine models
- [ ] Xbox machine models
- [ ] Nintendo machine models
- [ ] handheld machine models
- [ ] PC-compatible machine models
- [ ] platform-specific GPU implementations
- [ ] platform-specific audio implementations
- [ ] platform-specific input/controllers
- [ ] platform-specific storage and peripherals
- [ ] platform firmware/boot chains
- [ ] platform operating-system environments
- [ ] additional CPU architectures such as AArch64, MIPS and PowerPC
- [ ] JIT/recompiler

A checked foundation item means that the repository contains an implementation and corresponding tests. It does **not** mean every edge case of the real hardware architecture has already been verified.

## Roadmap

ArchiaEmu is being developed in stages.

### Phase 1 — Correct CPU foundation

**Current priority.**

1. Complete the x86-64 interpreter audit.
2. Reproduce every discovered architectural bug with a regression test.
3. Correct instruction semantics and decoding.
4. Expand exception, privilege and paging behavior.
5. Keep CPU and architectural tests independently runnable.
6. Require targeted tests and the complete CTest suite before considering each area stable.

### Phase 2 — Stable machine/core architecture

After the CPU foundation is sufficiently verified:

1. strengthen the Machine abstraction;
2. formalize the bus/address-space model;
3. improve device registration and MMIO;
4. define interrupt, DMA and timing interfaces;
5. make platform components replaceable without changing verified CPU behavior.

### Phase 3 — First complete machine target

Build a complete documented machine model using the reusable foundation.

The first machine target is intended to validate the architecture itself:

- CPU configuration
- physical memory map
- bus
- interrupts
- timers
- storage
- input
- display/GPU boundary
- firmware/boot path
- operating-system interface

The goal is to establish a repeatable pattern for adding additional machines rather than hard-coding one console into the emulator core.

### Phase 4 — Console platform families

Add concrete console families progressively.

Each platform must have its own:

- hardware model
- CPU configuration
- memory map
- buses
- devices
- firmware/boot process
- operating-system/runtime interfaces
- executable or ROM loading path
- compatibility tests

Shared components should be reused where the real hardware behavior allows it; platform-specific behavior must remain isolated.

### Phase 5 — Broader architecture support

Add additional CPU architectures when the common interfaces are stable enough to support them cleanly, including candidates such as:

- AArch64 / ARM
- MIPS
- PowerPC
- other architectures required by target platforms

### Phase 6 — Performance

Only after correctness and regression coverage are strong:

- interpreter optimization
- caching
- block execution
- JIT/recompilation
- parallelism where architecturally safe
- platform-specific acceleration

**Correctness, reproducibility and architectural fidelity take priority over performance.**

## Working rules

Development follows:

```text
OBSERVE
  ↓
REPRODUIS
  ↓
TEST
  ↓
CORRIGE
  ↓
BUILD
  ↓
TEST CIBLÉ
  ↓
SUITE COMPLÈTE
  ↓
CI
```

Rules:

- inspect the current implementation before changing it;
- do not fix behavior by assumption;
- when a bug is suspected, create a reproducer/regression test first;
- never delete or weaken an existing test to make a change pass;
- reread modified files after every change;
- inspect the resulting diff;
- do not claim a build, test or CI result without actual verification.

## What "complete" means

ArchiaEmu will not be considered a general multi-console emulator merely because it contains several CPU implementations.

A platform becomes a real ArchiaEmu target only when its relevant hardware/software environment is modeled and verified sufficiently to run actual software for that platform.

The long-term objective is therefore:

```text
Reliable reusable foundations
        ↓
Reusable machine architecture
        ↓
Complete platform models
        ↓
Multiple console families
        ↓
Large multi-platform emulator
```

The repository is the source of truth for what is actually implemented and tested.
