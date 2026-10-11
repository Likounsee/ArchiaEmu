# Roadmap and verified status

Percentages reflect the checked-in implementation and verified tests, not time spent or planned features.

| Phase | Current estimate | Remaining acceptance work |
| --- | ---: | --- |
| P0 — foundations | 70% | Verify the new dependency/package gates in CI; add property/fuzz tests when justified; establish release artifact validation |
| P1 — first console | 0% | Select target; implement hardware and instruction behavior; run reproducible authorized tests |
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
- [ ] Confirm the new dependency/package gates pass on all CI runners; release artifact validation remains.
- [x] Cross-platform CI matrix passed on Linux, Windows and macOS on commit `72d2460575a561ffc44d0348081491384aabc86f`.

No console is currently supported. Choose the first target only after auditing hardware specifications, implementation scope, and availability of lawful, reproducible test inputs.
