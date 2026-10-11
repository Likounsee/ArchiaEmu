# Testing policy

## Current checks

From `ArchiaEmu/`:

```sh
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked
```

The CI workflow runs formatting, tests and warning-free documentation generation on Linux, Windows and macOS for pushes and pull requests targeting `ArchiaEmu`; Clippy runs on Linux. A green build proves only that the checked-in code passes these checks on the listed runners; it does not prove console compatibility or native builds for every target architecture. The stable toolchain is declared in `rust-toolchain.toml`.

## Current test coverage

The core contract crate tests canonical and malformed identifiers (including whitespace, non-ASCII characters and invalid separators), default-false capability declarations, registry lookup and creation, duplicate registration without mutation, deterministic enumeration order, descriptor/key mismatch rejection, stable display messages for every public identifier, core-operation and registry error variant, empty-registry behavior, and explicit unsupported save/load-state behavior. Registry factory tests verify that the factory is invoked on each creation request. There are no CPU, bus, instruction, timing, graphics, audio, controller, content-loading or console compatibility tests yet.

## Future core tests

Each core should add focused instruction/register tests, memory and bus boundary tests, interrupt/timer behavior, peripheral behavior, malformed-content tests, save/restore tests, and authorized reference programs with deterministic expected results. Keep fixtures small, redistributable and provenance-documented.

Do not remove a failing regression test merely to make CI green. Record unavailable checks explicitly and keep benchmark results separate from correctness tests. New tests should cover both the expected result and the unchanged-state guarantee when an operation is rejected. Cross-platform CI validates the current host-independent crate only; platform-specific adapters must add their own native tests.
