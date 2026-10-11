# ArchiaEmu

**ArchiaEmu is an in-development, modular multi-console emulation project.** Its long-term goal is to host multiple independently testable emulation cores behind a shared application and tooling layer. The repository is currently at its initial foundation stage: no console is claimed as supported until a real core and a reproducible test corpus demonstrate that support.

## Goals

- Build new shared components primarily in **stable Rust**, emphasizing explicit errors, predictable resource use, and testability.
- Integrate **C or C++** only when a mature, legally reusable library or a concrete interoperability/performance need justifies the added build and maintenance cost.
- Keep emulation cores independent of the user interface, with documented boundaries for memory ownership, audio/video buffers, and errors.
- Validate each platform against reproducible tests rather than treating successful compilation as compatibility.
- Handle imported game images, firmware, configuration, and save files as untrusted input.

## Repository layout

- `README.md` — this project overview, kept at the repository root for GitHub.
- `ArchiaEmu/` — project source, Cargo workspace (when established), technical documentation, tests, fixtures, and development tools.
- `.github/workflows/` — repository-level automation, so GitHub Actions can discover workflows.

The Git metadata remains at the repository root. See the [technical project README](ArchiaEmu/README.md) once the project directory is present.

## Current status

**Stage: initial repository foundation.** The current branch contains only this README. There is no emulator core, frontend, audio/video pipeline, controller support, save-state implementation, or verified console compatibility in the current tree. No hardware platform is currently supported or experimental in code.

## Roadmap

| Phase | Focus | Status |
| --- | --- | --- |
| P0 | Repository structure, Rust workspace, CI, architecture, tests and licensing baseline | Not started |
| P1 | First real console core and reproducible program tests | Not started |
| P2 | Video, audio, input and stable emulation loop | Not started |
| P3 | Game library, persistent saves and robust save states | Not started |
| P4 | Reproducible benchmarks and measured optimization | Not started |
| P5 | A second independently tested platform | Not started |
| P6 | Gradual work on more complex systems | Not started |
| P7 | Security review, reproducible distribution and release process | Not started |

Phase completion will be based on checked-in implementation and verified acceptance tests, not on directory names or aspirational claims.

## Build and test

The current repository does not yet contain a Cargo workspace, so there are no valid project build or test commands to run yet. Once the workspace exists, development commands will be documented in [`ArchiaEmu/README.md`](ArchiaEmu/README.md) and the CI will invoke them from the `ArchiaEmu/` directory.

## Supported systems

None at this stage. Planned systems will be listed separately from systems with experimental, partial, or validated compatibility in `ArchiaEmu/docs/SUPPORTED_SYSTEMS.md`.

## Contributions

Development takes place on the `ArchiaEmu` branch. Do not commit project changes to `main`. Before proposing a change, run the documented formatting, test, and lint checks where available, describe any checks that could not be run, and avoid bundling unrelated changes.

## Legal and content policy

ArchiaEmu does not intend to distribute copyrighted game images, BIOS files, or firmware without authorization. Third-party cores and libraries must be evaluated for licensing, redistribution terms, security, maintenance, and platform support before integration.
