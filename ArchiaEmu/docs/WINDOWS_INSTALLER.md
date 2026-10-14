# Windows installer packaging

ArchiaEmu uses **Inno Setup 6** for its initial Windows x64 installer. The script is
`packaging/windows/ArchiaEmu.iss`; the CI workflow is
`.github/workflows/windows-installer.yml` at the repository root.

## Build from a clean Windows runner

The workflow builds the desktop crate in release mode, stages
`archiaemu-desktop.exe`, and creates a versioned `ArchiaEmu-Setup-<version>.exe`.
It also emits a SHA-256 sidecar file. Manually dispatch the workflow to download a
30-day workflow artifact, or push a `v*` tag to attach the installer and checksum
to a GitHub Release.

The workflow currently compiles the application from the repository's Cargo
manifest without a committed desktop-specific lockfile. This is not yet a fully
reproducible dependency-locked release; pin and commit the desktop lockfile before
claiming reproducible release builds.

## Installation and user data

- The installer offers English and French setup UI, a Start Menu shortcut, and an
  optional desktop shortcut.
- Installation is per-user by default, with an option to elevate if the user chooses
  a machine-wide destination.
- Application preferences remain in the Windows user configuration directory
  (`%APPDATA%\ArchiaEmu\desktop.conf`), not in the installation directory.
- Uninstall removes installed program files and shortcuts. It asks whether known
  settings/log/cache locations should also be removed; ROMs and arbitrary user files
  must never be deleted by uninstall.
- The app binary is currently the only staged runtime file. Validate this on a clean
  Windows VM before treating the package as complete; future resources or native
  runtime dependencies must be added to the staging rules.

## Release acceptance checklist

1. Run the workflow from a clean commit and confirm the build and installer compile succeed.
2. Verify the produced version matches `crates/archiaemu-desktop/Cargo.toml` and validate its SHA-256.
3. Install without Rust, Git, source files, or a build directory present.
4. Test Start Menu and optional desktop shortcuts, launch, preferences persistence, and restart.
5. Test upgrade over an older version and uninstall with both choices for user data.
6. Confirm no ROMs or files outside the explicitly named settings/log/cache locations are removed.
7. Record the Windows version, commit SHA, installer checksum, and actual results.

A workflow file or a generated installer alone does not prove the clean-machine
installation, upgrade, or uninstall scenarios have passed.
