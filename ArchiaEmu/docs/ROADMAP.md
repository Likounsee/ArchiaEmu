# Roadmap and verified status

Percentages reflect the checked-in implementation and verified tests, not time spent or planned features.

| Phase | Current estimate | Remaining acceptance work |
| --- | ---: | --- |
| P0 — foundations | 75% (estimate) | Validate module planner and disk-backed state store in CI; implement signed catalogue, secure installer trust checks, and release artifact validation |
| P1 — first console | 5% | Implement cartridge size/mapper validation, memory bus, LR35902 CPU and peripherals; run reproducible authorized tests |
| P2 — audio/video/input | 0% | Real platform output and controls; synchronization and latency tests |
| P3 — library/saves | 0% | Real content library, persistent saves and validated/versioned save states |
| P4 — performance | 0% | Baseline benchmarks, profiling and measured optimizations |
| P5 — second platform | 0% | Second independently tested core and host selection |
| P6 — complex systems | 0% | Per-system hardware, diagnostics and compatibility evidence |
| P7 — release | 0% | Security/legal review, reproducible packaging, support policy and release validation |

## P0 acceptance checklist

- [x] Root README describes the current implementation and its limitations.
- [x] Rust workspace and committed lockfile are present.
- [x] Shared core metadata and lifecycle/error contract is implemented.
- [x] Stable system identifier validation has unit tests.
- [x] Core registry supports unique IDs, deterministic enumeration, factory creation and descriptor/key consistency checks.
- [x] Tests cover duplicate/unknown IDs, mismatched factories and selected stable error messages.
- [x] Edge-case tests cover whitespace/non-ASCII identifiers, deterministic registration order, duplicate rejection without registry mutation, repeated factory creation, and explicit unsupported save/load-state operations.
- [x] Testing policy documents the CI scope and limitations of cross-platform checks.
- [x] CI generates API documentation with rustdoc warnings treated as errors on all three host operating systems.
- [x] Tests cover every public error enum's display messages and empty-registry behavior; repeated creation verifies factory invocation.
- [x] Repository-level CI workflow runs formatting, tests, Clippy, rustdoc, package checks and dependency policy checks.
- [x] GitHub Actions passed on commit `28d7d9ddbc91ea477155954304bda860cc8a6928`.
- [x] MIT and Apache-2.0 license texts are checked in; third-party policy is documented.
- [x] Architecture, interface, language, supported-system, graphics/audio, input, performance, testing, security, and legal documentation reviewed against the current source and repository metadata.
- [x] Dependency advisory, license and source-origin checks are configured through cargo-deny.
- [x] Workspace package-content validation is part of CI.
- [ ] Confirm dependency/package gates, disk-backed module-state tests, and platform-specific path handling pass on the latest CI run; release artifact validation remains.
- [x] Add a pure module catalogue/dependency planner with runtime-validated IDs, duplicate/missing dependency and cycle validation, topological installation order, shared dependency de-duplication, installed-module handling, and explicit in-development rejection.
- [x] Add a first-launch and module-state model for skip/completion status, deduplicated selection, installed versions, version updates and post-removal bookkeeping.
- [x] Add a versioned disk-backed state store with strict decoding, size limits, per-user path resolution, staged writes and backup recovery tests.
- [x] Add strict release-version parsing, app compatibility bounds, explicit OS/architecture targets, and structural validation for module manifest dependencies and artifact metadata.
- [x] Document first-launch selection, offline/skip behavior, secure staged installation requirements, later module management, and the distinction between a catalogue entry and a working emulator.
- [x] First target selected and isolated in a dedicated crate; header parser validates minimum length, ROM size code and header checksum.
- [x] Cross-platform CI matrix passed on Linux, Windows and macOS on commit `72d2460575a561ffc44d0348081491384aabc86f`.

The original Game Boy (DMG) is selected as the first target. The current crate only parses limited header metadata; it is not a functioning emulator. See `docs/GAME_BOY_DMG.md`.
