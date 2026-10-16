#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Durable per-ROM save-state files for the desktop frontend.

use std::fs::{self, OpenOptions};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

const MAX_STATE_FILE_BYTES: u64 = 64 * 1024 * 1024 + 256;

/// Resolves the stable save-state slot for a ROM under the user's configuration directory.
pub fn state_path(config_file: &Path, content_path: &Path) -> io::Result<PathBuf> {
    let config_dir = config_file.parent().ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "configuration path has no parent")
    })?;
    let canonical_content = fs::canonicalize(content_path)?;
    let fingerprint = stable_fingerprint(canonical_content.to_string_lossy().as_bytes());
    Ok(config_dir
        .join("states")
        .join(format!("{fingerprint:016x}.archiasave")))
}

/// Reads a bounded save-state file, returning None when no slot has been saved.
pub fn read_state(path: &Path) -> io::Result<Option<Vec<u8>>> {
    let metadata = match fs::symlink_metadata(path) {
        Ok(metadata) => metadata,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    if metadata.file_type().is_symlink() || !metadata.is_file() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "save-state path must be a regular file, not a symlink",
        ));
    }
    if metadata.len() > MAX_STATE_FILE_BYTES {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "save-state file exceeds the maximum supported size",
        ));
    }
    let mut file = OpenOptions::new().read(true).open(path)?;
    let mut bytes = Vec::with_capacity(metadata.len() as usize);
    file.take(MAX_STATE_FILE_BYTES + 1).read_to_end(&mut bytes)?;
    if bytes.len() as u64 > MAX_STATE_FILE_BYTES {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "save-state file exceeds the maximum supported size",
        ));
    }
    Ok(Some(bytes))
}

/// Atomically writes a bounded save-state file while preserving the previous slot on failure.
pub fn write_state(path: &Path, bytes: &[u8]) -> io::Result<()> {
    if bytes.is_empty() || bytes.len() as u64 > MAX_STATE_FILE_BYTES {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "save-state payload is empty or too large",
        ));
    }
    let parent = path.parent().ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "save-state path has no parent")
    })?;
    fs::create_dir_all(parent)?;
    reject_symlink_or_non_file(path)?;

    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let file_name = path.file_name().and_then(|name| name.to_str()).ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "save-state filename is not valid UTF-8")
    })?;
    let temporary = parent.join(format!(".{file_name}.tmp-{}-{nonce}", std::process::id()));
    let backup = parent.join(format!(".{file_name}.bak-{}-{nonce}", std::process::id()));

    let write_result = (|| {
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary)?;
        file.write_all(bytes)?;
        file.sync_all()?;
        Ok::<(), io::Error>(())
    })();
    if let Err(error) = write_result {
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }

    reject_symlink_or_non_file(path)?;
    let had_previous = match fs::symlink_metadata(path) {
        Ok(_) => {
            fs::rename(path, &backup)?;
            true
        }
        Err(error) if error.kind() == io::ErrorKind::NotFound => false,
        Err(error) => {
            let _ = fs::remove_file(&temporary);
            return Err(error);
        }
    };

    if let Err(error) = fs::rename(&temporary, path) {
        if had_previous {
            let _ = fs::rename(&backup, path);
        }
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }
    if had_previous {
        fs::remove_file(backup)?;
    }
    Ok(())
}

fn reject_symlink_or_non_file(path: &Path) -> io::Result<()> {
    match fs::symlink_metadata(path) {
        Ok(metadata) if metadata.file_type().is_symlink() || !metadata.is_file() => Err(
            io::Error::new(
                io::ErrorKind::InvalidInput,
                "save-state path must be a regular file, not a symlink",
            ),
        ),
        Ok(_) => Ok(()),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(error),
    }
}

fn stable_fingerprint(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xcbf29ce484222325_u64, |hash, byte| {
        (hash ^ u64::from(*byte)).wrapping_mul(0x100000001b3)
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temporary_directory(label: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system clock should be after the Unix epoch")
            .as_nanos();
        let path = std::env::temp_dir().join(format!(
            "archiaemu-state-files-{}-{label}-{nonce}",
            std::process::id()
        ));
        fs::create_dir_all(&path).expect("temporary directory should be created");
        path
    }

    #[test]
    fn state_file_round_trip_replaces_previous_bytes() {
        let root = temporary_directory("round-trip");
        let path = root.join("slot.archiasave");
        write_state(&path, b"first snapshot").unwrap();
        assert_eq!(read_state(&path).unwrap().as_deref(), Some(&b"first snapshot"[..]));
        write_state(&path, b"second snapshot").unwrap();
        assert_eq!(read_state(&path).unwrap().as_deref(), Some(&b"second snapshot"[..]));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn missing_slot_is_not_an_error_and_empty_writes_are_rejected() {
        let root = temporary_directory("missing");
        let path = root.join("missing.archiasave");
        assert_eq!(read_state(&path).unwrap(), None);
        assert_eq!(
            write_state(&path, b""),
            Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "save-state payload is empty or too large"
            ))
        );
        fs::remove_dir_all(root).unwrap();
    }
}
