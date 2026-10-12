# Core interface

The shared Rust contract lives in `crates/archiaemu-core/src/lib.rs`.

## Required metadata

A concrete core reports a stable `SystemId`, a display name, its implementation version, and explicitly declared `CoreCapabilities`. Capabilities must reflect tested functionality, not roadmap intent.

## Lifecycle

`EmulationCore` defines content loading, reset, execution until an observable event, and optional save-state operations. The common `CoreEvent` allows non-frame scheduling boundaries. Platform cores may expose additional typed interfaces for timing, debugger access, device configuration and richer audio/video delivery.

## Core registry

`CoreRegistry` maps unique `SystemId` values to factory functions. Registration rejects duplicate IDs and preserves registration order for enumeration. Calling `create` constructs a fresh instance and verifies that the instance's descriptor reports the same ID as the registry key. Unknown IDs and mismatched factories return explicit `RegistryError` variants; they do not silently select a fallback core.

Factories should be cheap to call and must not load user content or perform irreversible side effects. Registry validation is not a substitute for validating ROMs, firmware, or machine state inside a concrete core.

## Error and data rules

- Reject malformed or unsupported content with `CoreError::InvalidContent`.
- Report operations before initialization as `CoreError::NotInitialized`.
- Return `Unsupported` for absent features; never silently pretend to save or restore state.
- Validate a state fully before mutating machine state.
- Keep buffer ownership and lifetime explicit if audio/video buffers are added.
- Avoid panics for malformed external content.

## Audio/video/input boundary\n\nThe shared lifecycle contract deliberately does not prescribe a universal audio/video buffer or input representation yet. Before the host starts consuming these devices, define typed, owned frame/sample payloads and input events in a small additive contract, with explicit pixel/sample formats, dimensions/rates, buffer lifetime, timestamp/timing semantics, and reset behavior. Keep device-specific register semantics in each console crate; do not expose a native plugin ABI or build renderer/audio backends as a prerequisite for CPU work. The Game Boy CPU and bus can be developed independently while this host-facing boundary is specified.\n\n## Shared input and media data contracts\n\n`archiaemu_core::core_io` defines platform-neutral `InputEvent`/`InputControl`, owned RGBA8888 `VideoFrame` payloads, and interleaved signed-16-bit PCM `AudioBuffer` payloads. Constructors validate frame dimensions, exact pixel byte count, nonzero audio format, and complete interleaved sample frames. These are data contracts only: there is no host input mapper, renderer, audio device backend, pacing loop, or core-to-host media delivery trait yet. The core can be implemented without waiting for those backends. Console-specific button layouts and hardware registers remain in each system module.\n\n## Stability

This API is an initial internal contract and may evolve before the first core is selected. It is not a stable plugin ABI. A native C ABI will only be designed if a real native core is chosen.
