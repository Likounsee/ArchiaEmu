# Security baseline

All imported ROM images, firmware, save files, state files, metadata and configuration are untrusted input.

- Validate sizes, offsets, lengths and integer conversions before access or allocation.
- Put upper bounds on allocations driven by input data.
- Validate save-state version, target system ID and core version before restore.
- Parse into temporary structures and only mutate machine state after validation succeeds.
- Prefer explicit errors to panics on malformed external data.
- Keep unsafe Rust and FFI out of the shared core crate unless a specific, reviewed need exists.
- For native integrations, document pointer validity, ownership, buffer lengths, threading and error propagation.
- Avoid downloading or bundling copyrighted firmware or game images without permission.

This is a policy baseline, not a completed security audit. No hostile-input fuzzing or native boundary currently exists.
