# Core interface

The initial Rust contract lives in `crates/archiaemu-core/src/lib.rs`.

## Required metadata

A concrete core reports a stable `SystemId`, a display name, its implementation version, and explicitly declared `CoreCapabilities`. Capabilities must reflect tested functionality, not roadmap intent.

## Lifecycle

`EmulationCore` currently defines content loading, reset, execution until an observable event, and optional save-state operations. The common `CoreEvent` intentionally allows non-frame scheduling boundaries. Platform cores may expose additional typed interfaces for timing, debugger access, device configuration and richer audio/video delivery.

## Error and data rules

- Reject malformed or unsupported content with `CoreError::InvalidContent`.
- Report operations before initialization as `CoreError::NotInitialized`.
- Return `Unsupported` for absent features; never silently pretend to save or restore state.
- Validate a state fully before mutating machine state.
- Keep buffer ownership and lifetime explicit if audio/video buffers are added.
- Avoid panics for malformed external content.

## Stability

This API is an initial internal contract and may evolve before the first core is selected. Do not treat it as a stable plugin ABI. A native C ABI will only be designed if a real native core is chosen.
