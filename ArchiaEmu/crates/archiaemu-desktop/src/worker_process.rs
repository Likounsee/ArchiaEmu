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
use std::io::{self, Write};
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
    /// Framed IPC failed.
    Protocol(WorkerProtocolError),
}

impl std::fmt::Display for WorkerProcessError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidExecutablePath => f.write_str("worker executable must be an absolute file path"),
            Self::Process(error) => write!(f, "worker process operation failed: {error}"),
            Self::MissingPipe => f.write_str("worker process did not expose required IPC pipes"),
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
