# ArchiaEmu Desktop

The native desktop frontend is built with eframe/egui and uses the shared application controller rather than implementing a separate content/session pipeline.

## Current experience

- Overview dashboard with library/session/system counts and recent content.
- Multi-file ROM import through the native file picker, with a summary of imported and rejected files; the current selection follows the first successfully imported canonical path rather than a rejected file.
- Persisted library paths are revalidated on startup; unavailable or invalid paths are skipped with a recovery message so users can re-import files that moved or disappeared.
- Game library search by title or path, filters for .gb and .gbc, and sorting by title, size, or path.
- Per-game selection, readable file sizes, path details, launch, use-path, and remove-from-library actions.
- Session management, registered-system capability reporting, per-session video/audio capability labels, and persistent appearance preferences (light/dark theme, UI scale and customizable accent color).
- A dedicated Game page integrated into navigation and selected automatically after a successful launch.
- Pause/resume, stop and reset controls in the Game page, plus shortcuts to the library and session manager.
- A bounded host tick loop, newest-frame consumption, RGBA stride-aware conversion, a 64 MiB texture-upload cap, and a reusable nearest-neighbour egui texture.
- Semantic keyboard mapping (arrows, Z/X/A/S, Enter, Backspace); repeated key-downs are ignored, and held inputs are released when focus is lost or the user leaves the Game page.
- Cached video is cleared when sessions are switched or closed, and when the active session is stopped or reset from either the Game page or session manager; empty and media-error states are shown without synthesizing fake frames.
- User-facing empty states and import/launch error summaries.
- Light/dark appearance modes plus a persistent accent-color picker; the accent changes button/selection highlights while leaving light/dark surfaces intact.

The library accepts files through the shared library validator (regular file, non-empty, bounded size and duplicate protection). Importing a file does **not** certify that the ROM is valid or compatible; the launch pipeline performs format probing and the emulator core remains experimental.

## Architecture boundaries

- `main.rs` composes the application, renders egui navigation and owns the top-level app/controller lifecycle. Page rendering is split into `home_page.rs`, `library_page.rs`, `game_page.rs`, `sessions_page.rs`, `systems_page.rs` and `settings_page.rs`; keyboard/video adapters live in `game_view.rs`, and preference persistence lives in `preferences.rs`.
- Desktop lifecycle and input commands go through typed `ApplicationController` methods rather than directly mutating session internals. Session counts and read-only session view summaries also come from the controller, keeping page rendering away from runtime internals. The controller remains frontend-independent. Session summaries expose whether content is loaded; the session manager displays that state alongside lifecycle status, declared video/audio capabilities and the last core error.
- Localization is planned in P1: visible UI strings will move to stable translation keys backed by language catalogues (initially French and English), with a persistent language setting. Internal identifiers and developer diagnostics remain language-neutral.
- The core runtime owns session lifecycle rules, semantic input state and execution contracts. `ApplicationController::advance_session` owns bounded execution ticks and media-queue draining; the desktop converts the returned latest frame into a texture and renders it.
- New frontend features should extend the shared application/controller boundary before reaching into the host runtime. Keep console-specific behavior out of shared UI and controller modules.

## UI direction

The intended desktop experience is a focused emulation library, not a console-specific dashboard: a persistent left navigation rail, a compact top header with current page/session count, a searchable and sortable library, a dedicated session manager, a game surface with honest media-capability states, a systems/capabilities page, and simple host preferences. Keep actions close to the selected content, make empty/error states actionable, and avoid presenting unsupported cores or media as functional. See [`docs/DESKTOP_UI_PLAN.md`](../../docs/DESKTOP_UI_PLAN.md) for the written layout and implementation order.

## Validation status

The backend-tick, preference-persistence and video-conversion tests are committed, but they must not be considered validated until `cargo fmt`, workspace tests and the Linux, Windows and macOS desktop CI complete successfully. The desktop checks include formatting, compilation and unit tests.

## Run

From the ArchiaEmu/ workspace root:

```sh
cargo run -p archiaemu-desktop
```

The native picker supports Game Boy and Game Boy Color extensions (.gb, .gbc) and also offers an all-files filter. Imported paths and appearance settings are stored in the platform user configuration directory.

## Media pipeline status

The desktop now consumes bounded RGBA video frames from cores that advertise video output and uploads the latest frame to an egui texture. The host loop uses a 60 Hz cadence accumulator with bounded catch-up and a per-tick event budget; this is a frontend scheduling scaffold, not proof of accurate console timing. Arrow keys map to directions, Z/X/A/S to action buttons, Enter to Start and Backspace to Select. Inputs are forwarded only through the shared semantic input contract. Audio playback is not integrated yet. The current Game Boy core does not produce video frames or audio buffers, so the game surface remains an explicit placeholder for that core. A successful ROM launch does not imply playable-game compatibility.

### Préférences et tests de robustesse

Le parseur de préférences accepte la version 1 historique et la version 2 actuelle. Une version inconnue est rejetée explicitement; les champs malformés utilisent des valeurs par défaut sûres et un chemin échappé invalide est ignoré. Des tests couvrent aussi le premier démarrage sans fichier de configuration. Les tests ont été ajoutés sur la branche `ArchiaEmu`; leur exécution reste à confirmer par la CI.
