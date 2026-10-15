#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Minimal process boundary for an externally installed core worker.
//!
//! This is not an OS sandbox. The caller must authenticate the executable and its
//! parent directories against trusted installation metadata before spawning it.
//! Worker messages are bounded by the shared framing protocol.

use archiaemu_core::module_worker_protocol::{
    read_message, write_message, WorkerMessage, WorkerMessageKind, WorkerProtocolError,
};
use std::ffi::OsStr;
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};

/// Errors from worker executable validation, startup, or IPC.
#[derive(Debug)]
pub enum WorkerProcessError {
    /// The requested executable path is not absolute or does not name a file.
    InvalidExecutablePath,
    /// Process startup or lifecycle operation failed.
    Process(io::Error),
    /// Worker standard input/output pipes were not available.
    MissingPipe,
    /// The executable's SHA-256 did not match trusted installation metadata.
    ExecutableHashMismatch,
    /// Framed IPC failed.
    Protocol(WorkerProtocolError),
}

impl std::fmt::Display for WorkerProcessError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidExecutablePath => f.write_str("worker executable must be an absolute file path"),
            Self::Process(error) => write!(f, "worker process operation failed: {error}"),
            Self::MissingPipe => f.write_str("worker process did not expose required IPC pipes"),
            Self::ExecutableHashMismatch => {
                f.write_str("worker executable SHA-256 does not match trusted metadata")
            }
            Self::Protocol(error) => write!(f, "worker IPC failed: {error}"),
        }
    }
}

impl std::error::Error for WorkerProcessError {}

impl From<io::Error> for WorkerProcessError {
    fn from(error: io::Error) -> Self {
        Self::Process(error)
    }
}

impl From<WorkerProtocolError> for WorkerProcessError {
    fn from(error: WorkerProtocolError) -> Self {
        Self::Protocol(error)
    }
}

/// A child process connected to the bounded ArchiaEmu worker protocol.
pub struct WorkerProcess {
    child: Child,
    stdin: ChildStdin,
    stdout: ChildStdout,
    executable: PathBuf,
}

impl WorkerProcess {
    /// Starts a worker only when its executable SHA-256 matches trusted metadata.
    ///
    /// The expected digest must be 64 lowercase hexadecimal characters. The
    /// executable must still live in a directory protected from untrusted writes:
    /// portable path-based process APIs cannot eliminate every hash-to-spawn race.
    /// This verifies artifact identity, not publisher authenticity or OS sandboxing.
    pub fn spawn_verified<I, S>(
        executable: &Path,
        expected_sha256: &str,
        arguments: I,
    ) -> Result<Self, WorkerProcessError>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        if expected_sha256.len() != 64
            || !expected_sha256
                .bytes()
                .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte))
        {
            return Err(WorkerProcessError::ExecutableHashMismatch);
        }
        if !executable.is_absolute() || !executable.is_file() {
            return Err(WorkerProcessError::InvalidExecutablePath);
        }

        let mut file = File::open(executable)?;
        let mut hasher = Sha256::new();
        let mut buffer = [0_u8; 64 * 1024];
        loop {
            let read = file.read(&mut buffer)?;
            if read == 0 {
                break;
            }
            hasher.update(&buffer[..read]);
        }
        let actual = format!("{:x}", hasher.finalize());
        if actual != expected_sha256 {
            return Err(WorkerProcessError::ExecutableHashMismatch);
        }

        Self::spawn(executable, arguments)
    }

    /// Starts a worker at an absolute path with a cleared inherited environment.
    ///
    /// The caller is responsible for validating the executable's signature/hash,
    /// install location and provenance. This method intentionally does not search
    /// the executable search path, inherit environment variables, or load a library
    /// into the host.
    pub fn spawn<I, S>(executable: &Path, arguments: I) -> Result<Self, WorkerProcessError>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        if !executable.is_absolute() || !executable.is_file() {
            return Err(WorkerProcessError::InvalidExecutablePath);
        }
        let executable = executable.canonicalize()?;
        let mut command = Command::new(&executable);
        command
            .args(arguments)
            .env_clear()
            .current_dir(executable.parent().ok_or(WorkerProcessError::InvalidExecutablePath)?)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::null());

        let mut child = command.spawn()?;
        let stdin = child.stdin.take().ok_or(WorkerProcessError::MissingPipe)?;
        let stdout = child.stdout.take().ok_or(WorkerProcessError::MissingPipe)?;
        Ok(Self { child, stdin, stdout, executable })
    }

    /// Returns the canonical path of the executable launched for this worker.
    pub fn executable(&self) -> &Path {
        &self.executable
    }

    /// Sends one size-limited protocol message to the worker.
    pub fn send(&mut self, message: &WorkerMessage) -> Result<(), WorkerProcessError> {
        write_message(&mut self.stdin, message)?;
        Ok(())
    }

    /// Receives one size-limited protocol message from the worker.
    ///
    /// This call blocks until a complete message arrives or the worker's pipe closes.
    /// A supervising thread or platform-specific job object is still needed for hard
    /// timeouts and resource limits.
    pub fn receive(&mut self) -> Result<WorkerMessage, WorkerProcessError> {
        Ok(read_message(&mut self.stdout)?)
    }

    /// Requests an orderly shutdown without waiting indefinitely for the child.
    pub fn request_shutdown(&mut self) -> Result<(), WorkerProcessError> {
        self.send(&WorkerMessage::new(WorkerMessageKind::Shutdown, Vec::new())?)
    }

    /// Returns the exit status if the worker has exited.
    pub fn try_wait(&mut self) -> Result<Option<std::process::ExitStatus>, WorkerProcessError> {
        Ok(self.child.try_wait()?)
    }

    /// Terminates and reaps the worker immediately.
    pub fn terminate(&mut self) -> Result<(), WorkerProcessError> {
        match self.child.try_wait()? {
            Some(_) => Ok(()),
            None => {
                self.child.kill()?;
                self.child.wait()?;
                Ok(())
            }
        }
    }
}

impl Drop for WorkerProcess {
    fn drop(&mut self) {
        // Never leave an orphan worker behind when the host releases its handle.
        if self.child.try_wait().ok().flatten().is_none() {
            let _ = self.child.kill();
            let _ = self.child.wait();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{WorkerProcess, WorkerProcessError};
    use std::path::Path;

    #[test]
    fn refuses_executable_with_wrong_digest_before_startup() {
        let executable = std::env::current_exe().unwrap();
        let error = match WorkerProcess::spawn_verified(
            &executable,
            &"0".repeat(64),
            std::iter::empty::<&str>(),
        ) {
            Err(error) => error,
            Ok(_) => panic!("digest mismatch must prevent process startup"),
        };
        assert!(matches!(error, WorkerProcessError::ExecutableHashMismatch));
    }

    #[test]
    fn refuses_malformed_digest_and_relative_executable() {
        assert!(matches!(
            WorkerProcess::spawn_verified(
                Path::new("worker.exe"),
                "not-a-digest",
                std::iter::empty::<&str>(),
            ),
            Err(WorkerProcessError::ExecutableHashMismatch)
        ));
        assert!(matches!(
            WorkerProcess::spawn(Path::new("worker.exe"), std::iter::empty::<&str>()),
            Err(WorkerProcessError::InvalidExecutablePath)
        ));
    }
}
