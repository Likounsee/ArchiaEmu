# Architecture

## Current state

The repository currently contains a small Rust workspace and the `archiaemu-core` contract crate. There is no concrete console implementation or frontend. The contract is a starting boundary, not proof of emulation compatibility.

## Intended layers

1. **Application / frontend** — library browsing, launch orchestration, user settings, diagnostics, and host-side interface themes. It must not own console-specific hardware logic.
2. **Shared services** — configuration, content metadata, input mapping, save storage, audio/video host integration, presentation settings and logging, introduced when implemented functionality needs them.
3. **Core contract** — stable metadata, capability declarations, lifecycle errors and observable execution events. Graphics capabilities must be declared rather than inferred.
4. **Platform cores** — independent CPU, memory/bus, interrupt/timer, peripheral, graphics and audio implementations as required by each target.
5. **Presentation pipeline** — host-side scaling, aspect-ratio handling, filtering, color correction and optional overlays or redesigned menus. It must not be confused with the emulated GPU/video hardware or modify core timing.
6. **Native adapters** — optional C ABI boundaries for licensed C/C++ cores, with explicit ownership, buffer lifetime and error mapping.

## Dependency direction

The application may depend on shared services and core contracts. Concrete cores may depend on the minimal contract but must not depend on a graphical frontend. Avoid cycles between services and cores.

## Core contract decisions

- Each core exposes a stable lowercase system ID and an implementation version.
- Capabilities are explicit booleans and default to false; the host must not infer support from a system name.
- Execution returns an observable event rather than forcing every platform into a frame-per-call abstraction.
- Save-state methods default to an explicit unsupported error.
- External bytes are validated by the concrete core before machine state is changed.
- Platform-specific extensions remain possible when the common interface is insufficient.

## Console software interfaces and enhanced presentation

A console with a system interface may expose a dashboard, home menu, settings UI, media shell or other built-in software. This must be evaluated per system: some consoles have no general-purpose system UI, and faithful behavior may depend on original firmware or system software.

- Keep three modes conceptually distinct: execution of original system software when legally supplied by the user, independent faithful reimplementation of documented behavior, and optional modernized interface themes.
- Never imply a recreated interface is the original firmware. Track the source and confidence of behavioral details.
- Keep presentation enhancements in the host renderer when possible. A system-specific shader or reconstruction belongs behind an explicit capability and setting, with a faithful baseline available.
- Enhancement options may include integer scaling, aspect-ratio correction, filtering, color correction, and higher-resolution redrawn menus. 3D upscaling, texture replacement, widescreen patches or geometry fixes are system/game-specific and must not be promised universally.
- Test authentic and enhanced modes separately. Enhancements must be reversible and must not be required for correctness.
- Do not bundle copyrighted firmware, proprietary UI assets or console software without permission. Support user-provided lawful files where feasible, and independently recreate artwork/behavior when appropriate.

## Next architectural gates

Before selecting a first console, inspect reliable public specifications and legally usable test programs. Choose based on manageable scope and testability. Add real bus/CPU tests before creating a UI that could imply the system is already emulated.
