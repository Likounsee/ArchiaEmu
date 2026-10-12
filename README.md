# ArchiaEmu

**ArchiaEmu is an in-development, modular multi-console emulation project.** Its long-term goal is to host independently testable emulation cores behind a shared application and tooling layer. The repository is at the foundation stage; no console is claimed as supported until a real core and reproducible tests demonstrate that support.

## Goals

- Build shared components primarily in stable Rust, with explicit errors and testable boundaries.
- Integrate C or C++ only when a mature, legally reusable library or a concrete interoperability/performance need justifies it.
- Keep cores independent of the UI and make audio/video ownership and timing explicit.
- For consoles with software interfaces, aim to reproduce or independently recreate their dashboards, system menus, launchers and settings interfaces when feasible.
- Provide optional, reversible visual enhancements for older systems, while retaining a faithful original-presentation mode and clearly distinguishing recreated interfaces from original firmware.
- Validate platform behavior with reproducible tests rather than treating compilation as compatibility.
- Treat game images, firmware, configuration and save files as untrusted input.

## Repository layout

- `ArchiaEmu/` — Cargo workspace, source crates and technical documentation.
- `.github/workflows/` — GitHub Actions quality gates.
- `ArchiaEmu/docs/` — architecture, interface, roadmap, testing, security and legal notes.

## Current implementation

The shared Rust crate `archiaemu-core` defines validated system identifiers, capability metadata, a core registry, an independent-session lifecycle controller, bounded execution requests, common media payloads, and host-side input state with duplicate-transition suppression and explicit release recovery. Module selection/state persistence and host performance planning are also present as foundations. Runtime tests cover lifecycle transitions, error recovery, independent session creation, and shared input-state behavior.

The Game Boy module is one unified `game-boy` target with cartridge-header classification for DMG-only, dual-compatible, and CGB-only titles. This classification selects a planned hardware mode; it does not mean those modes are already emulated. The project still lacks a complete console implementation, real renderer/audio backend, application frontend, and verified end-to-end game compatibility. No system is currently claimed as supported.

## Build and test

Run from the project directory:

```sh
cd ArchiaEmu
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked
```

GitHub Actions runs formatting, locked tests and warning-free API documentation generation on pushes to the `ArchiaEmu` branch and on pull requests targeting it; Clippy runs on Linux.

## Roadmap

- **P0 — Foundations:** workspace, shared contracts, registry, CI, tests, documentation and licensing review.
- **P1 — First console: one unified Game Boy emulator:** expose one user-facing entry named **Game Boy / Game Boy Color** and one stable system ID (`game-boy`). Detect cartridge compatibility from the ROM header and select DMG or CGB hardware behavior internally; do not ship separate DMG and CGB emulators. Validate both modes against documented behavior and lawful test ROMs before claiming compatibility.
- **P2 — Runtime I/O:** integrate the common input state with a real host loop, then implement video presentation, audio playback and deterministic timing/synchronization.
- **Presentation/UI:** host-side interface recreation and optional visual enhancements, implemented per system only after a real core and renderer exist.
- **P3 — Library and saves:** content management, persistent data and validated save states.
- **P4 — Performance:** reproducible baselines, profiling and measured optimizations.
- **P5–P7 — Expansion and release:** second platform, more complex systems, security/legal review and reproducible packaging.

Progress is measured against checked-in implementation and verified acceptance tests, not planned features or directory names.

## Development and legal policy

Development changes belong on the `ArchiaEmu` branch; do not commit project changes to `main`. Do not distribute copyrighted game images, BIOS files or firmware without authorization. Third-party dependencies must be reviewed for license compatibility, redistribution terms, security, maintenance and platform support before integration.
