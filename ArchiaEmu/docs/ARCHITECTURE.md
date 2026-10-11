# Architecture

## Current state

The repository currently contains a small Rust workspace and the `archiaemu-core` contract crate. There is no concrete console implementation or frontend. The contract is a starting boundary, not proof of emulation compatibility.

## Intended layers

1. **Application / frontend** — library browsing, launch orchestration, user settings and diagnostics. It must not own console-specific hardware logic.
2. **Shared services** — configuration, content metadata, input mapping, save storage, audio/video host integration and logging, introduced when implemented functionality needs them.
3. **Core contract** — stable metadata, capability declarations, lifecycle errors and observable execution events.
4. **Platform cores** — independent CPU, memory/bus, interrupt/timer, peripheral, graphics and audio implementations as required by each target.
5. **Native adapters** — optional C ABI boundaries for licensed C/C++ cores, with explicit ownership, buffer lifetime and error mapping.

## Dependency direction

The application may depend on shared services and core contracts. Concrete cores may depend on the minimal contract but must not depend on a graphical frontend. Avoid cycles between services and cores.

## Core contract decisions

- Each core exposes a stable lowercase system ID and an implementation version.
- Capabilities are explicit booleans and default to false; the host must not infer support from a system name.
- Execution returns an observable event rather than forcing every platform into a frame-per-call abstraction.
- Save-state methods default to an explicit unsupported error.
- External bytes are validated by the concrete core before machine state is changed.
- Platform-specific extensions remain possible when the common interface is insufficient.

## Next architectural gates

Before selecting a first console, inspect reliable public specifications and legally usable test programs. Choose based on manageable scope and testability. Add real bus/CPU tests before creating a UI that could imply the system is already emulated.
