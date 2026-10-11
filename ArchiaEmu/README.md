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
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
```

The CI runs the same checks on pushes and pull requests targeting `ArchiaEmu`.

## Current components

- `crates/archiaemu-core`: shared metadata, capability declarations, core lifecycle contract, and error types. It is not itself a console emulator.
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

P0 is underway. The initial shared interface is a contract for future cores; it does not imply a console has been implemented. See `docs/ROADMAP.md` and `docs/SUPPORTED_SYSTEMS.md`.
