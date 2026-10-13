# ArchiaEmu Desktop (experimental)

This is an isolated eframe/egui frontend prototype. It is deliberately excluded from the main Cargo workspace until the native UI dependency stack has been validated on Linux, Windows, and macOS.

## Run

From the repository root:

```sh
cargo run --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml
```

The first build resolves the standalone prototype's dependency graph and creates its local Cargo lockfile. The main workspace's committed `ArchiaEmu/Cargo.lock` remains untouched.

## Prototype scope

- Browse for Game Boy ROM files with the native file picker, or enter a path manually.
- Import and forget content paths in the shared in-memory library.
- Launch content through the shared bounded validation pipeline.
- Show registered systems and their declared capabilities.
- Select, pause, resume, stop, reset, and close sessions.
- Surface launch failures and retained core diagnostics.

The prototype does not yet render video, play audio, schedule real-time execution, make the current Game Boy core playable. Launching a ROM is not a claim of compatibility.
