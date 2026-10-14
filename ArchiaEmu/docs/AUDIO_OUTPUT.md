# Host audio output

ArchiaEmu's desktop host accepts the common core media contract: signed 16-bit PCM (`i16`), interleaved by channel, with a non-zero sample rate and channel count. `AudioBuffer` validates that the number of samples forms complete channel frames before a core can return the buffer.

## Playback path

1. A core returns an owned `AudioBuffer` through `EmulationCore::take_audio_buffer`.
2. `ApplicationController::advance_session` drains at most 16 buffers per UI tick, preserving their order.
3. The desktop audio adapter converts each `i16` sample to normalized `f32` and submits it to the default host output device through rodio.
4. The host sink queues at most eight buffers before clearing excessive backlog. This intentionally favors low latency over playing stale audio after a stall.

The desktop opens audio output on a best-effort basis. If no output device is present or the host cannot initialize audio, emulation and video remain available and the media status reports the audio-device problem rather than crashing.

## Session lifecycle

- Pause pauses the host sink.
- Resume resumes it.
- Stop, reset, session switching, and session closure clear queued audio to prevent stale samples from leaking into another run.
- Audio buffers are drained even when a session is paused, but they are not submitted for playback while paused.
- Empty audio buffers are valid and are ignored by the host.

The current experimental Game Boy core does not yet synthesize audio. Therefore this integration establishes and tests the generic core-to-host playback path, but it does not claim that Game Boy games currently produce sound. A core must declare `audio_output` and actually return buffers before playback can be heard.

## Validation

Unit tests cover deterministic PCM collection through the application controller, signed-16-bit normalization, and the playback backlog threshold. The desktop CI workflow is configured to verify compilation and tests on Linux, macOS, and Windows; its result for this change still needs confirmation. Automated CI cannot guarantee audible output because hosted runners may not expose a physical or virtual audio device; manual smoke testing with a core that emits audio is still required.
