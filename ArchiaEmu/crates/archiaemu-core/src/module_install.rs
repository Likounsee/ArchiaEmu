#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Host-driven installation transaction orchestration.
//!
//! This module connects the durable journal to host-provided package operations.
//! It deliberately does not implement networking, archive formats, signatures,
//! or platform-specific activation. The backend must enforce those boundaries.

use core::fmt;
use std::io;
use std::path::{Path, PathBuf};

use crate::module_install_journal::{InstallJournal, InstallJournalError, InstallPhase};
use crate::module_management::ModuleId;

/// Operations supplied by the trusted host installer.
///
/// Implementations should make each operation idempotent where practical.
/// In particular, activation must be atomic or have its own recovery procedure.
pub trait ModuleInstallBackend {
    /// Downloads and verifies the package bytes for the exact module version.
    fn download_and_verify(&mut self, module: &ModuleId, version: &str) -> Result<(), String>;

    /// Extracts the verified package into an isolated staging location.
    fn extract_to_staging(&mut self, module: &ModuleId, version: &str) -> Result<(), String>;

    /// Activates the staged package at its final location.
    fn activate_staged(&mut self, module: &ModuleId, version: &str) -> Result<(), String>;

    /// Persists the installed module record after activation succeeded.
    fn persist_installed_state(&mut self, module: &ModuleId, version: &str) -> Result<(), String>;

    /// Removes temporary files for a transaction that has not committed.
    ///
    /// This must not delete or overwrite a previously installed working version.
    fn cleanup_uncommitted(&mut self, module: &ModuleId, version: &str) -> Result<(), String>;
}

/// Last operation attempted by an installation coordinator.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum InstallOperation {
    /// Initial journal creation or persistence.
    Journal,
    /// Download and artifact verification.
    Download,
    /// Package extraction into staging.
    Extraction,
    /// Activation of staged package.
    Activation,
    /// Persistence of installed state.
    StatePersistence,
    /// Removal of temporary transaction files.
    Cleanup,
}

/// Error returned by the installation coordinator.
#[derive(Debug)]
pub enum ModuleInstallError {
    /// The durable journal could not be read or written.
    Journal(InstallJournalError),
    /// An operation failed before activation; cleanup was attempted.
    OperationFailed {
        /// Operation that failed.
        operation: InstallOperation,
        /// Module ID.
        module: ModuleId,
        /// Host-provided failure details.
        message: String,
        /// Additional cleanup failure, if any.
        cleanup_error: Option<String>,
    },
    /// Activation completed or may have completed, so automatic cleanup is unsafe.
    RecoveryRequired {
        /// Operation that failed after activation began.
        operation: InstallOperation,
        /// Module ID.
        module: ModuleId,
        /// Host-provided failure details.
        message: String,
        /// Journal path that must be reconciled on startup.
        journal_path: PathBuf,
    },
}

impl fmt::Display for ModuleInstallError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Journal(error) => write!(f, "installation journal failed: {error}"),
            Self::OperationFailed { operation, module, message, cleanup_error } => {
                write!(f, "module '{module}' failed during {operation:?}: {message}")?;
                if let Some(cleanup) = cleanup_error {
                    write!(f, "; cleanup also failed: {cleanup}")?;
                }
                Ok(())
            }
            Self::RecoveryRequired { operation, module, message, journal_path } => write!(
                f,
                "module '{module}' failed during {operation:?}: {message}; reconcile journal at {} before retrying",
                journal_path.display()
            ),
        }
    }
}

impl std::error::Error for ModuleInstallError {}

impl From<InstallJournalError> for ModuleInstallError {
    fn from(error: InstallJournalError) -> Self {
        Self::Journal(error)
    }
}

/// Installs one already-planned module version through journaled milestones.
///
/// The journal is saved before the first backend operation and after every phase
/// transition. A failure before activation is marked failed and cleaned up.
/// A failure after activation leaves the journal at its last durable milestone
/// so startup recovery can reconcile the installed directory with persisted state.
pub fn install_module<B: ModuleInstallBackend>(
    backend: &mut B,
    journal_path: impl AsRef<Path>,
    module: ModuleId,
    version: impl Into<String>,
) -> Result<(), ModuleInstallError> {
    let journal_path = journal_path.as_ref();
    let version = version.into();
    let mut journal = InstallJournal::new(module.clone(), version.clone())?;
    journal.save(journal_path)?;

    advance_and_save(&mut journal, InstallPhase::Downloading, journal_path)?;
    if let Err(message) = backend.download_and_verify(&module, &version) {
        return Err(fail_before_activation(
            backend,
            &mut journal,
            journal_path,
            InstallOperation::Download,
            message,
        ));
    }

    advance_and_save(&mut journal, InstallPhase::Verified, journal_path)?;
    if let Err(message) = backend.extract_to_staging(&module, &version) {
        return Err(fail_before_activation(
            backend,
            &mut journal,
            journal_path,
            InstallOperation::Extraction,
            message,
        ));
    }

    advance_and_save(&mut journal, InstallPhase::Staged, journal_path)?;
    // Persist intent before touching the final location. If the process stops
    // mid-activation, recovery knows to inspect rather than blindly discard staging.
    advance_and_save(&mut journal, InstallPhase::Activating, journal_path)?;
    if let Err(message) = backend.activate_staged(&module, &version) {
        // Activation may have partially changed the final location. Preserve the
        // journal as Failed and require explicit recovery rather than deleting data.
        let _ = journal.advance(InstallPhase::Failed);
        let _ = journal.save(journal_path);
        return Err(ModuleInstallError::RecoveryRequired {
            operation: InstallOperation::Activation,
            module,
            message,
            journal_path: journal_path.to_path_buf(),
        });
    }

    advance_and_save(&mut journal, InstallPhase::Activated, journal_path)?;
    if let Err(message) = backend.persist_installed_state(&module, &version) {
        // Do not roll back or delete files: activation succeeded, and state may be
        // partially persisted. Startup must reconcile the durable Activated journal.
        return Err(ModuleInstallError::RecoveryRequired {
            operation: InstallOperation::StatePersistence,
            module,
            message,
            journal_path: journal_path.to_path_buf(),
        });
    }

    advance_and_save(&mut journal, InstallPhase::Committed, journal_path)?;
    Ok(())
}

fn advance_and_save(
    journal: &mut InstallJournal,
    phase: InstallPhase,
    path: &Path,
) -> Result<(), ModuleInstallError> {
    journal.advance(phase)?;
    journal.save(path)?;
    Ok(())
}

fn fail_before_activation<B: ModuleInstallBackend>(
    backend: &mut B,
    journal: &mut InstallJournal,
    journal_path: &Path,
    operation: InstallOperation,
    message: String,
) -> ModuleInstallError {
    let _ = journal.advance(InstallPhase::Failed);
    let journal_error = journal.save(journal_path).err();
    let cleanup_error = backend
        .cleanup_uncommitted(&journal.entry().module, &journal.entry().version)
        .err();

    if let Some(error) = journal_error {
        return ModuleInstallError::RecoveryRequired {
            operation: InstallOperation::Journal,
            module: journal.entry().module.clone(),
            message: format!("{message}; failed to persist failure state: {error}"),
            journal_path: journal_path.to_path_buf(),
        };
    }

    ModuleInstallError::OperationFailed {
        operation,
        module: journal.entry().module.clone(),
        message,
        cleanup_error,
    }
}

/// Returns the currently recorded phase from a journal file.
pub fn read_install_phase(path: impl AsRef<Path>) -> Result<InstallPhase, ModuleInstallError> {
    Ok(InstallJournal::load(path)?.entry().phase)
}

/// Convenience conversion for callers that want to report journal I/O failures
/// without depending on this module's error enum.
pub fn journal_io_error(error: io::Error) -> ModuleInstallError {
    ModuleInstallError::Journal(InstallJournalError::Io(error))
}
