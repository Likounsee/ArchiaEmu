# ADR 0001 — Desktop UI framework

- Status: accepted for the first prototype; reassess after the minimal cross-platform spike.
- Scope: P1.5 desktop shell only. Emulator cores and host contracts remain UI-independent.

## Decision

Use **eframe/egui** for the first native desktop prototype. Keep the existing terminal host available as a diagnostic frontend and keep content validation, session lifecycle, save-state handling, and capability reporting in the existing shared crates.

## Why

- The UI can be implemented in Rust without introducing a separate web frontend and bridge layer.
- egui's immediate-mode model fits the first shell: content library, registered systems and capabilities, session list, active-session controls, and diagnostics.
- eframe targets the three CI desktop platforms already exercised by this repository: Linux, Windows, and macOS.
- The UI can be prototyped without putting console-specific behavior into widgets; views consume host-facing models and commands instead.

## Constraints and acceptance checks

- Before adding the dependency to the workspace, build a minimal window on Linux, Windows, and macOS and verify the current stable Rust toolchain and the repository's MSRV policy.
- Review the exact selected crate versions, transitive dependencies, licenses, native system requirements, and packaging implications; the current CI dependency/license audit must remain green.
- Keep all core execution and filesystem validation out of UI callbacks. UI actions call a host/controller boundary and surface typed errors.
- Treat presentation settings (window size, scale, theme, audio device) separately from emulated hardware configuration.
- Do not claim real-time video/audio support until a frame is rendered and audio is played through a real backend with a bounded scheduling strategy.
- Add tests for empty library, import/launch errors, switching sessions, and shutdown before calling the desktop shell usable.

## Consequences

This is a direction for the prototype, not a claim that a desktop app already exists. The first deliverable is a small window plus the host/controller boundary; persistence of the content library, packaging, media presentation, and real-time scheduling remain separate work items.
