# Official module catalogue and signing plan

## Decision

ArchiaEmu uses a publisher-controlled Ed25519 signing identity for its official module catalogue. The private signing key is a release secret and must never be committed to this repository, included in the desktop installer, or exposed in build logs. The application pins only the corresponding public key and a stable key identifier.

## Trust bootstrap

1. Generate the Ed25519 key pair in a controlled release environment.
2. Store the private key in a protected CI secret with access restricted to trusted release workflows.
3. Review and pin the public key and key ID in the application source in a separately reviewed commit.
4. Publish the catalogue and detached signature as release artifacts on the official ArchiaEmu repository.
5. Require signature verification over the exact catalogue bytes before parsing or using catalogue fields.
6. Persist the highest accepted catalogue revision durably and reject rollback.
7. Document a key-rotation procedure before the first production catalogue is shipped.

A placeholder key, test verifier, HTTPS-only URL, or SHA-256 digest is not a substitute for an authentic signature. Until the production public key is pinned and a cryptographic verifier is wired into the application, external module installation must remain disabled.

## Package verification

A module package is accepted only when all checks pass:

- The catalogue signature validates against a pinned trusted key.
- The module ID, version, host API, target platform and dependencies pass schema and compatibility checks.
- The download uses HTTPS and an approved official distribution host.
- The downloaded byte count and SHA-256 digest match the signed catalogue.
- Archive paths are checked for traversal, absolute paths, platform-specific path tricks, and duplicate/conflicting entries before extraction.
- Extraction happens in a private staging directory; activation and installed-state persistence are journaled.
- Failed pre-activation operations clean up only their own staging files. Failures after activation begins require explicit journal recovery rather than blind deletion.

## Runtime isolation

Native Rust trait objects do not form a stable plugin ABI. ArchiaEmu must not load arbitrary third-party Rust DLLs directly into the desktop process. External cores should use a versioned, bounded IPC protocol in a worker process. The host must validate every message, cap payload sizes, enforce timeouts and lifecycle limits, and treat worker crashes as recoverable failures. Process separation alone is not a complete OS sandbox; stronger Windows isolation must be implemented and tested before being advertised as a security guarantee.

## Release gate

Do not publish a production module catalogue or advertise dynamic module installation until all of the following exist:

- A pinned production public key and reviewed key-rotation procedure.
- A real Ed25519 implementation (not the test sentinel in unit tests).
- Automated signature/tamper/unknown-key/rollback tests.
- A trusted catalogue publication workflow using a protected signing secret.
- A verified package installer and recovery tests.
- A worker-process protocol and executable isolation tests for external cores.

The built-in Game Boy core remains optional through the `game-boy-core` Cargo feature. A build without that feature must report that no Game Boy core is bundled rather than implying that an external core can already be installed.
