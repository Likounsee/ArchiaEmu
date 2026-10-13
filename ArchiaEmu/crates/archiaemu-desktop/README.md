# ArchiaEmu Desktop

The native desktop frontend is built with eframe/egui and uses the shared application controller rather than implementing a separate content/session pipeline.

## Current experience

- Overview dashboard with library/session/system counts and recent content.
- Multi-file ROM import through the native file picker, with a summary of imported and rejected files.
- Game library search by title or path, filters for .gb and .gbc, and sorting by title, size, or path.
- Per-game selection, readable file sizes, path details, launch, use-path, and remove-from-library actions.
- Session management, registered-system capability reporting, and persistent appearance preferences.
- A dedicated Game page integrated into navigation and selected automatically after a successful launch.
- Pause/resume, stop and reset controls in the Game page, plus shortcuts to the library and session manager.
- A bounded host tick loop, newest-frame consumption, RGBA stride-aware conversion, a 64 MiB texture-upload cap, and a reusable nearest-neighbour egui texture.
- Semantic keyboard mapping (arrows, Z/X/A/S, Enter, Backspace); repeated key-downs are ignored, and held inputs are released when focus is lost or the user leaves the Game page.
- Cached video is cleared when sessions are switched or closed; empty and media-error states are shown without synthesizing fake frames.
- User-facing empty states and import/launch error summaries.

The library accepts files through the shared library validator (regular file, non-empty, bounded size and duplicate protection). Importing a file does **not** certify that the ROM is valid or compatible; the launch pipeline performs format probing and the emulator core remains experimental.

## Validation status

The new game-loop integration and video conversion tests are committed, but they must not be considered validated until the current Linux, Windows and macOS CI run completes successfully. The desktop checks include formatting, compilation and unit tests.

## Run

From the ArchiaEmu/ workspace root:

```sh
cargo run -p archiaemu-desktop
```

The native picker supports Game Boy and Game Boy Color extensions (.gb, .gbc) and also offers an all-files filter. Imported paths and appearance settings are stored in the platform user configuration directory.

## Media pipeline status

The desktop now consumes bounded RGBA video frames from cores that advertise video output and uploads the latest frame to an egui texture. The host loop uses a 60 Hz cadence accumulator with bounded catch-up and a per-tick event budget; this is a frontend scheduling scaffold, not proof of accurate console timing. Arrow keys map to directions, Z/X/A/S to action buttons, Enter to Start and Backspace to Select. Inputs are forwarded only through the shared semantic input contract. Audio playback is not integrated yet. The current Game Boy core does not produce video frames or audio buffers, so the game surface remains an explicit placeholder for that core. A successful ROM launch does not imply playable-game compatibility.
