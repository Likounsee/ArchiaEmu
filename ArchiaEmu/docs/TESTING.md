# Testing policy

## Current checks

From `ArchiaEmu/`:

```sh
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
```

The CI workflow runs these checks on pushes and pull requests targeting `ArchiaEmu`. A green build proves only that the checked-in code passes these checks; it does not prove console compatibility.

## Current test coverage

The core contract crate tests canonical system identifiers, malformed identifier rejection, default-false capability declarations, and explicit unsupported save-state behavior. There are no CPU, bus, instruction, timing, graphics, audio, controller, content-loading or console compatibility tests yet.

## Future core tests

Each core should add focused instruction/register tests, memory and bus boundary tests, interrupt/timer behavior, peripheral behavior, malformed-content tests, save/restore tests, and authorized reference programs with deterministic expected results. Keep fixtures small, redistributable and provenance-documented.

Do not remove a failing regression test merely to make CI green. Record unavailable checks explicitly and keep benchmark results separate from correctness tests.
