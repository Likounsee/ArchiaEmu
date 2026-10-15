# ArchiaEmu — technical overview

ArchiaEmu is currently a repository foundation, not a usable emulator. No console core is implemented yet, and no game compatibility is claimed.

## Repository and toolchain

- Main language for new shared components: stable Rust.
- C/C++: considered only for justified, licensed native integrations.
- Assembly: only for measured, architecture-specific optimizations.
- Project root: this directory. Repository-level README and GitHub Actions remain one level above.

Once the workspace has been established, run these commands from this directory:

```sh
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked
cargo package --workspace --locked --no-verify
```

The CI runs formatting, locked tests, warning-free documentation generation and package-content checks on pushes and pull requests targeting `ArchiaEmu`. Dependency advisories, license policy and source provenance are checked with cargo-deny; Clippy runs on Linux.

## Quick desktop demo

From the repository root, launch the native application with:

```sh
cargo run --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml
```

On Linux, install the native desktop development libraries listed in `.github/workflows/ci.yml` first. On Windows, the **Windows installer** GitHub Actions workflow builds a standalone installer; use its artifact from a successful run rather than presenting a local debug build as a release.

For a short demonstration, show the home page, import a local `.gb`/`.gbc` file, browse and filter the library, open session management, and change/save appearance preferences. Use your own legally obtained test file. **Important limitation:** this demonstrates the desktop application shell and cartridge metadata inspection, not playable emulation. An experimental LR35902 instruction subset exists, but the CPU, memory bus, graphics, timers/interrupts, input, audio, and mapper support are incomplete; no game compatibility is claimed.

## Current components

- `crates/archiaemu-core`: shared metadata, capability declarations, core lifecycle contract, and error types.
- `crates/archiaemu-game-boy`: experimental Game Boy cartridge-header parser. CPU, bus, PPU, audio, and runnable-game support are not implemented.
- `docs/`: architecture, language decisions, interface, roadmap, compatibility, media/input plans, testing, performance, security, and legal constraints.
- `.github/workflows/ci.yml` at the repository root: Rust quality gates.

## Development principles

1. Do not advertise a system as supported until implementation and reproducible tests exist.
2. Keep UI dependencies out of emulation cores.
3. Validate all untrusted content and state data at boundaries.
4. Prefer explicit errors to panics for malformed external input.
5. Add dependencies only with a concrete need and license/maintenance review.
6. Benchmark before optimizing hot paths.
7. Preserve deterministic behavior where the target hardware and core permit it.

## Status

P0 is underway. The initial shared interface is a contract for future cores; it does not imply a console has been implemented. See `docs/ROADMAP.md`, `docs/SUPPORTED_SYSTEMS.md`, and `docs/GAME_BOY_DMG.md` for the first-target decision and explicit scope.
