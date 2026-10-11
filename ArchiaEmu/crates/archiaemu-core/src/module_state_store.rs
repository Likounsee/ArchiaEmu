#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Versioned, dependency-free persistence for first-launch and module state.

use core::fmt;
use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

use crate::module_management::ModuleId;
use crate::module_state::{FirstLaunchStatus, InstalledModule, ModuleManagerState};

/// Maximum accepted state-file size (4 MiB).
pub const MAX_STATE_FILE_BYTES: u64 = 4 * 1024 * 1024;
const HEADER: &str = "ARCHIAEMU-MODULE-STATE\t1";
static TEMP_SEQUENCE: AtomicU64 = AtomicU64::new(0);

/// A malformed or unsupported persisted state.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum StateFormatError {
    /// The header or format version is unsupported.
    UnsupportedFormat,
    /// A record is malformed or missing a required field.
    MalformedRecord,
    /// A module identifier is invalid.
    InvalidModuleId,
    /// A version string has invalid hexadecimal or UTF-8 encoding.
    InvalidVersion,
    /// A module occurs more than once in the same collection.
    DuplicateModule,
}

impl fmt::Display for StateFormatError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnsupportedFormat => f.write_str("unsupported module state format"),
            Self::MalformedRecord => f.write_str("malformed module state record"),
            Self::InvalidModuleId => f.write_str("invalid module ID in persisted state"),
            Self::InvalidVersion => f.write_str("invalid version encoding in persisted state"),
            Self::DuplicateModule => f.write_str("duplicate module in persisted state"),
        }
    }
}
impl std::error::Error for StateFormatError {}

/// An error encountered while loading or saving module state.
#[derive(Debug)]
pub enum ModuleStateStoreError {
    /// Filesystem operation failed.
    Io(io::Error),
    /// State file exceeds the size limit.
    TooLarge,
    /// Persisted data is invalid.
    Format(StateFormatError),
}
impl fmt::Display for ModuleStateStoreError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(error) => write!(f, "module state I/O error: {error}"),
            Self::TooLarge => f.write_str("module state file exceeds 4 MiB"),
            Self::Format(error) => write!(f, "invalid module state: {error}"),
        }
    }
}
impl std::error::Error for ModuleStateStoreError {}
impl From<io::Error> for ModuleStateStoreError {
    fn from(value: io::Error) -> Self { Self::Io(value) }
}
impl From<StateFormatError> for ModuleStateStoreError {
    fn from(value: StateFormatError) -> Self { Self::Format(value) }
}

impl ModuleManagerState {
    /// Encodes state in deterministic version-1 UTF-8 records.
    ///
    /// Installed version strings are hexadecimal-encoded to prevent control
    /// characters from changing record boundaries.
    pub fn encode(&self) -> String {
        let mut output = format!(
            "{HEADER}\nstatus\t{}\n",
            match self.first_launch_status() {
                FirstLaunchStatus::NotStarted => "not-started",
                FirstLaunchStatus::Skipped => "skipped",
                FirstLaunchStatus::Completed => "completed",
            }
        );
        for id in self.selected_modules() {
            output.push_str(&format!("selected\t{}\n", id.as_str()));
        }
        for module in self.installed_modules() {
            output.push_str(&format!("installed\t{}\t", module.id.as_str()));
            for byte in module.version.as_bytes() {
                use core::fmt::Write as _;
                write!(&mut output, "{byte:02x}").expect("writing to String cannot fail");
            }
            output.push('\n');
        }
        output
    }

    /// Decodes version-1 state, rejecting duplicate or ambiguous records.
    pub fn decode(input: &str) -> Result<Self, StateFormatError> {
        let mut lines = input.lines();
        if lines.next() != Some(HEADER) {
            return Err(StateFormatError::UnsupportedFormat);
        }
        let mut status = None;
        let mut selected = Vec::new();
        let mut installed: Vec<InstalledModule> = Vec::new();
        for line in lines {
            if line.is_empty() {
                return Err(StateFormatError::MalformedRecord);
            }
            let fields: Vec<&str> = line.split('\t').collect();
            match fields.as_slice() {
                ["status", value] if status.is_none() => {
                    status = Some(match *value {
                        "not-started" => FirstLaunchStatus::NotStarted,
                        "skipped" => FirstLaunchStatus::Skipped,
                        "completed" => FirstLaunchStatus::Completed,
                        _ => return Err(StateFormatError::MalformedRecord),
                    });
                }
                ["selected", raw_id] if status.is_some() => {
                    let id = ModuleId::new(*raw_id)
                        .map_err(|_| StateFormatError::InvalidModuleId)?;
                    if selected.contains(&id) {
                        return Err(StateFormatError::DuplicateModule);
                    }
                    selected.push(id);
                }
                ["installed", raw_id, raw_version] if status.is_some() => {
                    let id = ModuleId::new(*raw_id)
                        .map_err(|_| StateFormatError::InvalidModuleId)?;
                    if installed.iter().any(|item| item.id == id) {
                        return Err(StateFormatError::DuplicateModule);
                    }
                    if raw_version.len() % 2 != 0 {
                        return Err(StateFormatError::InvalidVersion);
                    }
                    let mut bytes = Vec::with_capacity(raw_version.len() / 2);
                    for pair in raw_version.as_bytes().chunks_exact(2) {
                        let high = nibble(pair[0]).ok_or(StateFormatError::InvalidVersion)?;
                        let low = nibble(pair[1]).ok_or(StateFormatError::InvalidVersion)?;
                        bytes.push((high << 4) | low);
                    }
                    let version = String::from_utf8(bytes)
                        .map_err(|_| StateFormatError::InvalidVersion)?;
                    if version.trim().is_empty() {
                        return Err(StateFormatError::InvalidVersion);
                    }
                    installed.push(InstalledModule { id, version });
                }
                _ => return Err(StateFormatError::MalformedRecord),
            }
        }
        let status = status.ok_or(StateFormatError::MalformedRecord)?;
        let mut state = ModuleManagerState::new();
        state.set_selection(selected);
        match status {
            FirstLaunchStatus::NotStarted => {}
            FirstLaunchStatus::Skipped => state.mark_setup_skipped(),
            FirstLaunchStatus::Completed => state.mark_setup_completed(),
        }
        for module in installed {
            state.record_installed(module.id, module.version)
                .map_err(|_| StateFormatError::InvalidVersion)?;
        }
        Ok(state)
    }
}

fn nibble(byte: u8) -> Option<u8> {
    match byte {
        b'0'..=b'9' => Some(byte - b'0'),
        b'a'..=b'f' => Some(byte - b'a' + 10),
        _ => None,
    }
}

/// Disk-backed state store for a caller-selected application-data path.
///
/// Saves stage and flush a same-directory temporary file. A backup is retained
/// while replacing the primary state; callers must serialize concurrent writes.
pub struct ModuleStateStore {
    path: PathBuf,
}
impl ModuleStateStore {
    /// Creates a store at the exact path supplied by the application.
    pub fn new(path: impl Into<PathBuf>) -> Self { Self { path: path.into() } }

    /// Returns the configured state path.
    pub fn path(&self) -> &Path { &self.path }

    /// Loads persisted state, or returns a fresh profile when no file exists.
    pub fn load(&self) -> Result<ModuleManagerState, ModuleStateStoreError> {
        match read_state(&self.path) {
            Ok(state) => Ok(state),
            Err(primary) => {
                let backup = backup_path(&self.path);
                match read_state(&backup) {
                    Ok(state) => Ok(state),
                    Err(ModuleStateStoreError::Io(error))
                        if error.kind() == io::ErrorKind::NotFound
                            && matches!(
                                &primary,
                                ModuleStateStoreError::Io(primary_error)
                                    if primary_error.kind() == io::ErrorKind::NotFound
                            ) => Ok(ModuleManagerState::new()),
                    Err(ModuleStateStoreError::Io(error))
                        if error.kind() == io::ErrorKind::NotFound => Err(primary),
                    Err(backup_error) => Err(backup_error),
                }
            }
        }
    }

    /// Saves state using a flushed temporary file and recoverable backup.
    pub fn save(&self, state: &ModuleManagerState) -> Result<(), ModuleStateStoreError> {
        let parent = self.path.parent()
            .filter(|p| !p.as_os_str().is_empty())
            .unwrap_or_else(|| Path::new("."));
        fs::create_dir_all(parent)?;
        let sequence = TEMP_SEQUENCE.fetch_add(1, Ordering::Relaxed);
        let temp = self.path.with_extension(format!(
            "tmp-{}-{sequence}", std::process::id()
        ));
        let backup = backup_path(&self.path);
        let mut file = OpenOptions::new().write(true).create_new(true).open(&temp)?;
        let write_result = file.write_all(state.encode().as_bytes())
            .and_then(|()| file.sync_all());
        drop(file);
        if let Err(error) = write_result {
            let _ = fs::remove_file(&temp);
            return Err(error.into());
        }

        if backup.exists() {
            fs::remove_file(&backup)?;
        }
        let had_original = self.path.exists();
        if had_original {
            fs::rename(&self.path, &backup)?;
        }
        if let Err(error) = fs::rename(&temp, &self.path) {
            if had_original {
                let _ = fs::rename(&backup, &self.path);
            }
            let _ = fs::remove_file(&temp);
            return Err(error.into());
        }
        if let Ok(directory) = File::open(parent) {
            let _ = directory.sync_all();
        }
        Ok(())
    }
}

fn read_state(path: &Path) -> Result<ModuleManagerState, ModuleStateStoreError> {
    let mut file = File::open(path)?;
    let size = file.metadata()?.len();
    if size > MAX_STATE_FILE_BYTES {
        return Err(ModuleStateStoreError::TooLarge);
    }
    let mut bytes = Vec::with_capacity(size as usize);
    file.take(MAX_STATE_FILE_BYTES + 1).read_to_end(&mut bytes)?;
    if bytes.len() as u64 > MAX_STATE_FILE_BYTES {
        return Err(ModuleStateStoreError::TooLarge);
    }
    let text = std::str::from_utf8(&bytes)
        .map_err(|_| ModuleStateStoreError::Format(StateFormatError::MalformedRecord))?;
    ModuleManagerState::decode(text).map_err(Into::into)
}
fn backup_path(path: &Path) -> PathBuf { path.with_extension("bak") }

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temp_path() -> PathBuf {
        let unique = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_nanos();
        std::env::temp_dir().join(format!(
            "archiaemu-module-state-{}-{unique}.state", std::process::id()
        ))
    }

    #[test]
    fn codec_round_trips_status_selection_and_control_characters_in_version() {
        let mut state = ModuleManagerState::new();
        state.set_selection([
            ModuleId::new("game-boy").unwrap(),
            ModuleId::new("nes").unwrap(),
        ]);
        state.record_installed(
            ModuleId::new("game-boy").unwrap(), "1.2\t0\nβ"
        ).unwrap();
        state.mark_setup_completed();
        assert_eq!(ModuleManagerState::decode(&state.encode()).unwrap(), state);
    }

    #[test]
    fn codec_rejects_unknown_version_malformed_and_duplicate_records() {
        assert_eq!(
            ModuleManagerState::decode("ARCHIAEMU-MODULE-STATE\t2\nstatus\tnot-started\n"),
            Err(StateFormatError::UnsupportedFormat)
        );
        assert_eq!(
            ModuleManagerState::decode("ARCHIAEMU-MODULE-STATE\t1\nselected\tgb\n"),
            Err(StateFormatError::MalformedRecord)
        );
        assert_eq!(
            ModuleManagerState::decode(
                "ARCHIAEMU-MODULE-STATE\t1\nstatus\tnot-started\nselected\tgb\nselected\tgb\n"
            ),
            Err(StateFormatError::DuplicateModule)
        );
    }

    #[test]
    fn store_persists_and_recovers_backup_after_corruption() {
        let path = temp_path();
        let store = ModuleStateStore::new(&path);
        assert!(store.load().unwrap().should_show_first_launch());
        let mut first = ModuleManagerState::new();
        first.mark_setup_skipped();
        store.save(&first).unwrap();
        let mut second = ModuleManagerState::new();
        second.mark_setup_completed();
        store.save(&second).unwrap();
        fs::remove_file(&path).unwrap();
        assert_eq!(store.load().unwrap(), first);
        fs::write(&path, b"corrupt").unwrap();
        assert_eq!(store.load().unwrap(), first);
        let _ = fs::remove_file(&path);
        let _ = fs::remove_file(backup_path(&path));
    }
}
