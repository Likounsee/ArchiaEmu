# Supported systems

## Current verified status

**No console is currently emulated or supported.** The Game Boy crate currently parses a limited set of cartridge header fields only; it has no CPU, memory bus, peripherals, renderer, audio, or input implementation.

| System | Status | Evidence |
| --- | --- | --- |
| Nintendo Game Boy (DMG) | Planned / cartridge-header parser experimental | Header parser unit tests only; no instruction execution or game compatibility |

## Status definitions

- **Planned:** selected as a future target; no working core.
- **Experimental:** core-related code exists but behavior is incomplete or unverified.
- **Boots tests:** starts some documented test content.
- **Partially functional:** defined test subset passes, with documented gaps.
- **Validated compatibility:** a published test suite and method pass reproducibly.
- **Extended compatibility:** broad representative corpus and regression suite results are published.

A crate name, successful compilation, header parsing, or GUI entry does not count as console support. Future entries must identify test-suite provenance, versions/hashes where licensing permits, expected results, actual results, and known failures.
