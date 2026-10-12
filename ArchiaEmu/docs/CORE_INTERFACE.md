# Core interface

The shared Rust contract lives in `crates/archiaemu-core/src/lib.rs`.

## Required metadata

A concrete core reports a stable `SystemId`, a display name, its implementation version, and explicitly declared `CoreCapabilities`. Capabilities must reflect tested functionality, not roadmap intent.

## Lifecycle

`EmulationCore` defines content loading, reset, execution until an observable event, and optional save-state operations. The common `CoreEvent` allows non-frame scheduling boundaries. Platform cores may expose additional typed interfaces for timing, debugger access, device configuration and richer audio/video delivery.

## Core registry

`CoreRegistry` maps unique `SystemId` values to factory functions. Registration rejects duplicate IDs and preserves registration order for enumeration. Calling `create` constructs a fresh instance and verifies that the instance's descriptor reports the same ID as the registry key. Unknown IDs and mismatched factories return explicit `RegistryError` variants; they do not silently select a fallback core.

Factories should be cheap to call and must not load user content or perform irreversible side effects. Registry validation is not a substitute for validating ROMs, firmware, or machine state inside a concrete core.

## Generic content envelope

`content::ContentImage` owns a non-empty payload that has passed an explicit
caller-provided maximum-size check. Hosts can use `ContentImage::read_path` for filesystem paths or
`ContentImage::read_from` for streams. Both read while consuming at most the
configured limit plus one byte;
the extra byte detects oversize input without buffering the whole source. A read
failure discards the partial payload. Hosts should construct this envelope before
calling `EmulationSession::load_validated_content`. The limit must be selected
by policy rather than guessed globally, since legitimate content sizes vary by
system. This common check does not detect file formats, validate firmware,
authenticate a source, or replace the core's format-specific parser. A core
must still validate its input before mutating machine state. The raw
`load_content(&[u8])` API remains available for internal/test callers, so hosts
handling untrusted files should prefer the validated path.

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
- Transition reference: `Empty -> Running` on successful load; `Running -> Paused` on pause; `Paused -> Running` on resume; `Running|Paused|Halted -> Running` on successful reset; `Running -> Halted` on a `Halted` event. Successful reload from any loaded state returns to `Running`.
- Invalid pause/resume/reset transitions return `SessionError::InvalidTransition` and do not change the lifecycle state. A core error is propagated as `SessionError::Core`; the session state is only committed after successful load/reset, and a run error leaves the session state unchanged.
- Successful content loading enters `Running`. A failed load leaves the session state unchanged.
- `pause` and `resume` only accept their corresponding source states.
- A `Halted` event moves the session to `Halted`; execution and input are then rejected until reset succeeds.
- Reset requires loaded content and returns the session to `Running` only after the core reset succeeds.
- Semantic input may be sent while running or paused. Media can be retrieved while loaded, including while paused or halted.

`EmulationRuntime` combines the registry with session creation: it enumerates registered systems and opens a fresh `EmulationSession` for a requested `SystemId`. Each created session owns a separate core instance; unknown IDs and factory/descriptor mismatches are explicit errors. `EmulationSession::run_for_events(max_events)` offers a bounded host-loop primitive: the budget counts core event calls (not cycles or elapsed time), and execution stops early at any event other than `Yielded` so the host can process media, debugger, or halt boundaries. A zero budget performs no execution. Each individual core call must still return at an observable boundary.

The runtime deliberately does not implement a wall-clock scheduler, pacing, renderer, audio device, or thread model. A future host loop can build on these explicit transitions without embedding console-specific assumptions in the shared core crate.

## Host capabilities and performance policy

The shared `host_profile` module separates host facts from user preferences. A host application supplies the logical CPU count, known available-memory estimate, and whether an accelerated video backend is usable. `PerformancePolicy::plan` then derives a worker-thread ceiling, a hardware/software video preference, a conservative memory ceiling, and permission for optional enhancements.

This is deliberately a policy model, not hardware detection: no operating-system or GPU probing is performed by the core crate yet. The host must validate any reported measurements and connect the plan to real schedulers, allocators, and renderer backends. The worker count is an upper bound for independent host work, not a claim that a core's emulated CPU can be parallelized. A software-video fallback remains available when acceleration is absent or not preferred.

The policy reserves at most 75% of reported available memory and honors a lower explicit user budget. If available memory is unknown, it remains unknown unless the user provides a budget. Zero CPU reports and zero worker limits are normalized to one worker. Optional enhancements are only permitted by preference; this does not claim that a core implements them. None of these choices may change emulated hardware semantics, guest timing, or deterministic results.

## Stability


This API is an initial internal contract and may evolve before the first core is selected. It is not a stable plugin ABI. A native C ABI will only be designed if a real native core is chosen.


## Compatibilité de l'API hôte/cœur

La compatibilité hôte/cœur est versionnée séparément de `core_version` via `CoreApiVersion`. La règle actuelle exige une version majeure identique et une version mineure requise inférieure ou égale à celle de l'hôte (`CURRENT_CORE_API_VERSION`). `EmulationRuntime::create_session` refuse un cœur incompatible avant de créer sa session.
