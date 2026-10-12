# Graphics and audio

## Current state

No video renderer, audio backend, synchronization loop or frame/sample buffers are implemented.

## Design constraints

- Keep host rendering/audio APIs outside hardware emulation logic.
- Model console timing according to each target's hardware; do not assume a universal frame rate or frame-based execution model.
- Make buffer ownership, sample format, pixel format, stride, lifetime and thread ownership explicit.
- Avoid unbounded queues. Measure queue depth and latency.
- Treat device initialization and runtime errors as visible failures, not silent no-ops.
- Provide deterministic headless paths for automated tests when possible.

## Validation

A future pipeline must test output dimensions/formats, boundary cases, audio sample counts, synchronization drift, device-loss handling, and prolonged sessions. Performance claims require reproducible measurements on named hardware/software configurations.
\n\n## Shared data types\n\nThe core crate now provides owned `VideoFrame` and `AudioBuffer` payload types in `core_io.rs`. Video is currently defined as row-major RGBA8888 with exact dimensions and no implicit row padding. Audio is interleaved signed 16-bit PCM with an explicit sample rate and channel count. These contracts make payload size and ownership explicit, but do not yet connect a core to a renderer, audio backend, or synchronized host loop. No core currently claims video or audio output capability.\n