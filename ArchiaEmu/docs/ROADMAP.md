# Roadmap and verified status

Percentages reflect the current checked-in implementation and tests, not effort spent or planned features.

| Phase | Current estimate | Remaining acceptance work |
| --- | ---: | --- |
| P0 — foundations | 20% | Verify CI on GitHub; lock/toolchain policy; complete root structure, licensing inventory, architecture review and broader error/serialization tests |
| P1 — first console | 0% | Select target; implement hardware and instruction behavior; run reproducible authorized tests |
| P2 — audio/video/input | 0% | Real platform output and controls; synchronization and latency tests |
| P3 — library/saves | 0% | Real content library, persistent saves and validated/versioned save states |
| P4 — performance | 0% | Baseline benchmarks, profiling and measured optimizations |
| P5 — second platform | 0% | Second independently tested core and host selection |
| P6 — complex systems | 0% | Per-system hardware, diagnostics and compatibility evidence |
| P7 — release | 0% | Security/legal review, reproducible packaging, support policy and release validation |

## P0 acceptance checklist

- [x] Root README created first and states the real project status.
- [x] Technical README and initial workspace are present.
- [x] Initial core metadata/lifecycle contract is implemented.
- [x] Identifier validation and capability/error behavior have unit tests.
- [x] Repository-level CI workflow is defined.
- [x] Initial architecture and language decisions are documented.
- [ ] GitHub Actions completes successfully on the current commit.
- [ ] License files and third-party inventory are finalized.
- [ ] The complete documentation set is reviewed against real code.
- [ ] Cross-platform build strategy is verified.

No system is currently supported. The first console remains unselected until source/specification and test availability are audited.
