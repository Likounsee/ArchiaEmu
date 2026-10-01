# MyPS5Emu

Educational PS5-emulator research project written in C++20 with CMake.

## Current milestone: 0.1

* ELF64 header validation
* Basic emulator orchestration
* Guest memory allocation stub
* CPU state stub
* Loader smoke test

This is **not** a working PS5 emulator yet. The project is intentionally built incrementally.

## Build (Windows / Visual Studio)

```powershell
cmake -S . -B build -G Ninja -DCMAKE\_BUILD\_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Or with a Visual Studio generator:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Run

```powershell
.\\build\\myps5emu.exe path\\to\\some-elf64-file
```

The loader currently reads only the ELF64 header. It does not load executable segments or execute guest code yet.

## Planned milestones

1. ELF64 program-header parsing
2. Guest virtual-memory mapping
3. CPU instruction decoder/interpreter
4. Syscall layer
5. Threading primitives
6. JIT/recompiler
7. GPU command processing
8. Shader translation to SPIR-V/Vulkan
9. Input/audio/filesystem services
10. Debugger and compatibility tooling

