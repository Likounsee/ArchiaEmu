# Roadmap and verified status

Percentages reflect the checked-in implementation and verified tests, not time spent or planned features.

| Phase | Current estimate | Remaining acceptance work |
| --- | ---: | --- |
| P0 — foundations | 45% | Verify the new Linux/Windows/macOS CI matrix; complete documentation review; expand robustness tests and tooling policy |
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
- [x] Repository-level CI workflow runs formatting, tests and Clippy.
- [x] GitHub Actions passed on commit `28d7d9ddbc91ea477155954304bda860cc8a6928`.
- [x] MIT and Apache-2.0 license texts are checked in; third-party policy is documented.
- [ ] The complete documentation set is reviewed against real code.
- [x] Cross-platform CI matrix is configured for Linux, Windows and macOS; successful runs are still required to verify it.

No console is currently supported. Choose the first target only after auditing hardware specifications, implementation scope, and availability of lawful, reproducible test inputs.
