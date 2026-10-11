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
