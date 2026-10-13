#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Frontend-independent application state shared by terminal and desktop hosts.

use std::error::Error;
use std::fmt;
use std::path::{Path, PathBuf};

use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeError, SessionId};
use archiaemu_core::CoreRegistry;

use crate::library::{ContentEntry, ContentLibrary, ContentLibraryError};

/// Owns the state that every ArchiaEmu frontend must share.
pub struct ApplicationController {
    host: HostRuntime,
    library: ContentLibrary,
    active_session: Option<SessionId>,
}

/// Errors returned when a frontend asks the controller to launch a library item.
#[derive(Debug)]
pub enum ApplicationError {
    /// The requested one-based library index does not exist.
    UnknownLibraryEntry(usize),
    /// The shared host rejected a session operation.
    Host(HostRuntimeError),
}

impl fmt::Display for ApplicationError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownLibraryEntry(index) => {
                write!(f, "no content library entry at index {index}")
            }
            Self::Host(error) => fmt::Display::fmt(error, f),
        }
    }
}

impl Error for ApplicationError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::UnknownLibraryEntry(_) => None,
            Self::Host(error) => Some(error),
        }
    }
}

impl From<HostRuntimeError> for ApplicationError {
    fn from(error: HostRuntimeError) -> Self {
        Self::Host(error)
    }
}

impl ApplicationController {
    /// Creates a frontend-neutral application state around an empty host runtime.
    pub fn new(host: HostRuntime) -> Self {
        Self {
            host,
            library: ContentLibrary::new(),
            active_session: None,
        }
    }

    /// Borrows the host runtime for read-only frontend diagnostics.
    pub fn host(&self) -> &HostRuntime {
        &self.host
    }

    /// Mutably borrows the host runtime for capability-specific commands.
    ///
    /// Prefer controller lifecycle methods for launching, selecting, and closing
    /// sessions so that the active-session selection remains synchronized.
    pub fn host_mut(&mut self) -> &mut HostRuntime {
        &mut self.host
    }

    /// Borrows the content catalog.
    pub fn library(&self) -> &ContentLibrary {
        &self.library
    }

    /// Mutably borrows the content catalog for import and removal operations.
    pub fn library_mut(&mut self) -> &mut ContentLibrary {
        &mut self.library
    }

    /// Returns the currently selected session, if any.
    pub const fn active_session(&self) -> Option<SessionId> {
        self.active_session
    }

    /// Returns the stable path at a one-based library index.
    pub fn library_entry_path(&self, index: usize) -> Option<PathBuf> {
        if index == 0 {
            return None;
        }
        self.library
            .entries()
            .nth(index - 1)
            .map(|entry| entry.path().to_path_buf())
    }

    /// Imports content into the catalog without starting an emulation session.
    pub fn import_content(
        &mut self,
        path: impl AsRef<Path>,
        max_bytes: u64,
    ) -> Result<&ContentEntry, ContentLibraryError> {
        self.library.import(path, max_bytes)
    }

    /// Removes a content entry without affecting any session already using it.
    pub fn forget_content(
        &mut self,
        path: impl AsRef<Path>,
    ) -> Result<ContentEntry, ContentLibraryError> {
        self.library.remove(path)
    }

    /// Launches a content path through the shared validation pipeline and selects it.
    pub fn launch_content_path(
        &mut self,
        path: impl AsRef<Path>,
        limits: ContentLimits,
        probes: &[&dyn ContentFormatProbe],
        registry: &CoreRegistry,
    ) -> Result<SessionId, HostRuntimeError> {
        let id = self
            .host
            .launch_content_path(path, limits, probes, registry)?;
        self.active_session = Some(id);
        Ok(id)
    }

    /// Launches a catalog item by its one-based index and selects its new session.
    pub fn launch_library_entry(
        &mut self,
        index: usize,
        limits: ContentLimits,
        probes: &[&dyn ContentFormatProbe],
        registry: &CoreRegistry,
    ) -> Result<SessionId, ApplicationError> {
        let path = self
            .library_entry_path(index)
            .ok_or(ApplicationError::UnknownLibraryEntry(index))?;
        self.launch_content_path(path, limits, probes, registry)
            .map_err(ApplicationError::Host)
    }

    /// Selects an existing session, rejecting stale IDs without changing selection.
    pub fn select_session(&mut self, id: SessionId) -> Result<(), HostRuntimeError> {
        self.host.session(id)?;
        self.active_session = Some(id);
        Ok(())
    }

    /// Closes a session and selects the oldest remaining session when necessary.
    pub fn close_session(
        &mut self,
        id: SessionId,
    ) -> Result<(), HostRuntimeError> {
        self.host.close_session(id)?;
        if self.active_session == Some(id) {
            self.active_session = self.host.session_ids().into_iter().next();
        }
        Ok(())
    }

    /// Closes every session and clears the active selection.
    pub fn close_all_sessions(&mut self) -> usize {
        self.active_session = None;
        self.host.close_all_sessions()
    }

    /// Provides disjoint mutable access for a frontend command dispatcher.
    ///
    /// Prefer the typed controller methods for lifecycle operations. This escape
    /// hatch exists for commands that need direct access to capability-specific APIs.
    pub fn parts_mut(
        &mut self,
    ) -> (&mut HostRuntime, &mut ContentLibrary, &mut Option<SessionId>) {
        (&mut self.host, &mut self.library, &mut self.active_session)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use archiaemu_game_boy::GameBoyCore;

    fn controller() -> ApplicationController {
        ApplicationController::new(HostRuntime::new(Default::default()).unwrap())
    }

    #[test]
    fn selecting_and_closing_sessions_keeps_active_selection_consistent() {
        let mut app = controller();
        let first = app
            .host_mut()
            .create_session(Box::new(GameBoyCore::new()))
            .unwrap();
        let second = app
            .host_mut()
            .create_session(Box::new(GameBoyCore::new()))
            .unwrap();

        app.select_session(first).unwrap();
        app.select_session(second).unwrap();
        assert_eq!(app.active_session(), Some(second));

        app.close_session(second).unwrap();
        assert_eq!(app.active_session(), Some(first));
        app.close_session(first).unwrap();
        assert_eq!(app.active_session(), None);
        assert!(app.host().is_empty());
    }

    #[test]
    fn closing_all_sessions_clears_selection_and_is_idempotent() {
        let mut app = controller();
        let id = app
            .host_mut()
            .create_session(Box::new(GameBoyCore::new()))
            .unwrap();
        app.select_session(id).unwrap();

        assert_eq!(app.close_all_sessions(), 1);
        assert_eq!(app.active_session(), None);
        assert_eq!(app.close_all_sessions(), 0);
    }

    #[test]
    fn selecting_unknown_session_does_not_change_the_active_session() {
        let mut app = controller();
        let id = app
            .host_mut()
            .create_session(Box::new(GameBoyCore::new()))
            .unwrap();
        let stale = app
            .host_mut()
            .create_session(Box::new(GameBoyCore::new()))
            .unwrap();
        app.select_session(id).unwrap();
        app.close_session(stale).unwrap();

        assert!(app.select_session(stale).is_err());
        assert_eq!(app.active_session(), Some(id));
    }

    #[test]
    fn library_index_errors_are_explicit_and_do_not_create_sessions() {
        let mut app = controller();
        let error = app
            .launch_library_entry(
                1,
                ContentLimits::new(1024),
                &[],
                &CoreRegistry::new(),
            )
            .unwrap_err();

        assert!(matches!(error, ApplicationError::UnknownLibraryEntry(1)));
        assert!(app.host().is_empty());
        assert_eq!(app.active_session(), None);
    }
}
