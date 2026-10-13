# ArchiaEmu Desktop

The native desktop frontend is built with eframe/egui and uses the shared application controller rather than implementing a separate content/session pipeline.

## Current experience

- Overview dashboard with library/session/system counts and recent content.
- Multi-file ROM import through the native file picker, with a summary of imported and rejected files.
- Game library search by title or path, filters for .gb and .gbc, and sorting by title, size, or path.
- Per-game selection, readable file sizes, path details, launch, use-path, and remove-from-library actions.
- Session management, registered-system capability reporting, and persistent appearance preferences.
- A dedicated game view with a bounded host tick loop, RGBA frame-to-texture upload, nearest-neighbour scaling, semantic keyboard input mapping, and input release on window focus loss.
- User-facing empty states and import/launch error summaries.

The library accepts files through the shared library validator (regular file, non-empty, bounded size and duplicate protection). Importing a file does **not** certify that the ROM is valid or compatible; the launch pipeline performs format probing and the emulator core remains experimental.

## Run

From the ArchiaEmu/ workspace root:

```sh
cargo run -p archiaemu-desktop
```

The native picker supports Game Boy and Game Boy Color extensions (.gb, .gbc) and also offers an all-files filter. Imported paths and appearance settings are stored in the platform user configuration directory.

## Media pipeline status

The desktop now consumes bounded RGBA video frames from cores that advertise video output and uploads the latest frame to an egui texture. The host loop uses a 60 Hz cadence accumulator with bounded catch-up and a per-tick event budget; this is a frontend scheduling scaffold, not proof of accurate console timing. Arrow keys map to directions, Z/X/A/S to action buttons, Enter to Start and Backspace to Select. Inputs are forwarded only through the shared semantic input contract. Audio playback is not integrated yet, and the current Game Boy core does not provide a playable video/audio implementation, so the game surface may remain a placeholder until a core produces media.
