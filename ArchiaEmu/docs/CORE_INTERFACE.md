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
- Keep audio/video buffer ownership and lifetime explicit at the host boundary.
- Avoid panics for malformed external content.

## Audio/video/input boundary

The common contract uses additive, typed methods on `EmulationCore` so the host can exchange input and media payloads without depending on a console crate:

- `send_input(InputEvent)` accepts semantic pressed/released transitions; a core maps them to its own hardware.
- `take_video_frame()` transfers ownership of one complete `VideoFrame`, or returns `None` when no frame is waiting.
- `take_audio_buffer()` transfers ownership of one `AudioBuffer`, or returns `None` when no audio is waiting.
- The default methods return `CoreError::Unsupported`, so existing cores do not claim functionality they have not implemented.
- `VideoFrame` currently validates non-zero dimensions and exact RGBA8888 payload length. `AudioBuffer` validates a non-zero sample rate/channel count and complete interleaved signed-16-bit PCM sample frames.

These contracts do not implement a host input mapper, renderer, audio device backend, scheduler, pacing loop, or timestamp synchronization. The host must treat `CoreEvent::FrameReady` and `CoreEvent::AudioReady` as signals to retrieve payloads, not as proof that a payload will always be available. Console-specific button layouts, video timing and audio generation remain the responsibility of each system module.

## Shared session runtime

`runtime::EmulationSession` owns one core and enforces host-facing lifecycle transitions independently of the emulated system:

- A new session starts in `Empty`; execution and media retrieval are rejected until content loads successfully.
- Successful content loading enters `Running`. A failed load leaves the session state unchanged.
- `pause` and `resume` only accept their corresponding source states.
- A `Halted` event moves the session to `Halted`; execution and input are then rejected until reset succeeds.
- Reset requires loaded content and returns the session to `Running` only after the core reset succeeds.
- Semantic input may be sent while running or paused. Media can be retrieved while loaded, including while paused or halted.

The session deliberately does not implement a wall-clock scheduler, pacing, renderer, audio device, or thread model. A future host loop can build on these explicit transitions without embedding console-specific assumptions in the shared core crate.

## Stability


This API is an initial internal contract and may evolve before the first core is selected. It is not a stable plugin ABI. A native C ABI will only be designed if a real native core is chosen.
