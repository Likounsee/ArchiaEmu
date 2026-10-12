#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Console-independent lifecycle controller for a single emulation session.

use core::fmt;

use crate::core_io::{AudioBuffer, InputEvent, VideoFrame};
use crate::{CoreDescriptor, CoreError, CoreEvent, EmulationCore};

/// Observable lifecycle state of an emulation session.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SessionState {
    /// No content has been loaded successfully.
    Empty,
    /// Content is loaded and execution is enabled.
    Running,
    /// Content is loaded but execution is paused.
    Paused,
    /// The core reported that the emulated machine halted.
    Halted,
}

/// An invalid lifecycle operation or an error reported by a core.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum SessionError {
    /// The operation is not valid in the current session state.
    InvalidTransition {
        /// Name of the requested operation.
        operation: &'static str,
        /// Current lifecycle state.
        state: SessionState,
    },
    /// The core rejected the requested operation.
    Core(CoreError),
}

impl fmt::Display for SessionError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidTransition { operation, state } => {
                write!(formatter, "cannot {operation} while session is {state:?}")
            }
            Self::Core(error) => fmt::Display::fmt(error, formatter),
        }
    }
}

impl std::error::Error for SessionError {}

impl From<CoreError> for SessionError {
    fn from(error: CoreError) -> Self {
        Self::Core(error)
    }
}

/// Owns one core and enforces consistent host-facing lifecycle transitions.
///
/// This type deliberately does not schedule wall-clock time, create threads,
/// render frames, or play audio. It provides a deterministic lifecycle boundary
/// that a future application loop can drive.
pub struct EmulationSession {
    core: Box<dyn EmulationCore>,
    state: SessionState,
}

impl EmulationSession {
    /// Wraps a fresh core in an initially empty session.
    pub fn new(core: Box<dyn EmulationCore>) -> Self {
        Self {
            core,
            state: SessionState::Empty,
        }
    }

    /// Returns the core's immutable descriptor.
    pub fn descriptor(&self) -> CoreDescriptor {
        self.core.descriptor()
    }

    /// Returns the current session lifecycle state.
    pub const fn state(&self) -> SessionState {
        self.state
    }

    /// Loads content and starts execution if the core accepts it.
    ///
    /// On failure, the session state is left unchanged. Core implementations
    /// are required to validate content before mutating machine state.
    pub fn load_content(&mut self, content: &[u8]) -> Result<(), SessionError> {
        self.core.load_content(content)?;
        self.state = SessionState::Running;
        Ok(())
    }

    /// Pauses a running session without discarding machine state.
    pub fn pause(&mut self) -> Result<(), SessionError> {
        self.require_state("pause", SessionState::Running)?;
        self.state = SessionState::Paused;
        Ok(())
    }

    /// Resumes a paused session.
    pub fn resume(&mut self) -> Result<(), SessionError> {
        self.require_state("resume", SessionState::Paused)?;
        self.state = SessionState::Running;
        Ok(())
    }

    /// Resets a loaded machine and resumes execution.
    pub fn reset(&mut self) -> Result<(), SessionError> {
        if self.state == SessionState::Empty {
            return Err(self.invalid_transition("reset"));
        }
        self.core.reset()?;
        self.state = SessionState::Running;
        Ok(())
    }

    /// Runs the core to its next observable event while the session is running.
    pub fn run_until_event(&mut self) -> Result<CoreEvent, SessionError> {
        self.require_state("run", SessionState::Running)?;
        let event = self.core.run_until_event()?;
        if event == CoreEvent::Halted {
            self.state = SessionState::Halted;
        }
        Ok(event)
    }

    /// Sends a semantic input transition while a loaded machine is active.
    ///
    /// Input is allowed while paused so a host can reconcile held controls
    /// before resuming. Input is rejected before loading or after a halt.
    pub fn send_input(&mut self, event: InputEvent) -> Result<(), SessionError> {
        if !matches!(self.state, SessionState::Running | SessionState::Paused) {
            return Err(self.invalid_transition("send input"));
        }
        self.core.send_input(event)?;
        Ok(())
    }

    /// Retrieves the next video frame from a loaded session, if one is waiting.
    pub fn take_video_frame(&mut self) -> Result<Option<VideoFrame>, SessionError> {
        self.require_loaded("retrieve video")?;
        Ok(self.core.take_video_frame()?)
    }

    /// Retrieves the next audio buffer from a loaded session, if one is waiting.
    pub fn take_audio_buffer(&mut self) -> Result<Option<AudioBuffer>, SessionError> {
        self.require_loaded("retrieve audio")?;
        Ok(self.core.take_audio_buffer()?)
    }

    fn require_state(
        &self,
        operation: &'static str,
        expected: SessionState,
    ) -> Result<(), SessionError> {
        if self.state == expected {
            Ok(())
        } else {
            Err(self.invalid_transition(operation))
        }
    }

    fn require_loaded(&self, operation: &'static str) -> Result<(), SessionError> {
        if self.state == SessionState::Empty {
            Err(self.invalid_transition(operation))
        } else {
            Ok(())
        }
    }

    fn invalid_transition(&self, operation: &'static str) -> SessionError {
        SessionError::InvalidTransition {
            operation,
            state: self.state,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{CoreCapabilities, SystemId};

    struct TestCore {
        loaded: bool,
        calls: usize,
        next_event: CoreEvent,
    }

    impl EmulationCore for TestCore {
        fn descriptor(&self) -> CoreDescriptor {
            CoreDescriptor {
                system_id: SystemId::new("runtime-test").unwrap(),
                display_name: "Runtime test core",
                core_version: "0.0.0-test",
                capabilities: CoreCapabilities::default(),
            }
        }

        fn load_content(&mut self, content: &[u8]) -> Result<(), CoreError> {
            if content.is_empty() {
                return Err(CoreError::InvalidContent("empty test content"));
            }
            self.loaded = true;
            Ok(())
        }

        fn reset(&mut self) -> Result<(), CoreError> {
            if !self.loaded {
                return Err(CoreError::NotInitialized);
            }
            self.calls = 0;
            self.next_event = CoreEvent::Yielded;
            Ok(())
        }

        fn run_until_event(&mut self) -> Result<CoreEvent, CoreError> {
            if !self.loaded {
                return Err(CoreError::NotInitialized);
            }
            self.calls += 1;
            Ok(self.next_event)
        }
    }

    fn session() -> EmulationSession {
        EmulationSession::new(Box::new(TestCore {
            loaded: false,
            calls: 0,
            next_event: CoreEvent::Yielded,
        }))
    }

    #[test]
    fn execution_is_blocked_until_content_is_loaded() {
        let mut session = session();
        assert_eq!(session.state(), SessionState::Empty);
        assert_eq!(
            session.run_until_event(),
            Err(SessionError::InvalidTransition {
                operation: "run",
                state: SessionState::Empty,
            })
        );
        assert!(session.load_content(&[]).is_err());
        assert_eq!(session.state(), SessionState::Empty);
    }

    #[test]
    fn pause_resume_and_reset_follow_valid_transitions() {
        let mut session = session();
        session.load_content(&[1]).unwrap();
        assert_eq!(session.state(), SessionState::Running);
        session.pause().unwrap();
        assert_eq!(session.state(), SessionState::Paused);
        assert!(session.run_until_event().is_err());
        session.resume().unwrap();
        assert_eq!(session.run_until_event().unwrap(), CoreEvent::Yielded);
        session.reset().unwrap();
        assert_eq!(session.state(), SessionState::Running);
    }

    #[test]
    fn halted_event_updates_session_state() {
        let mut core = TestCore {
            loaded: false,
            calls: 0,
            next_event: CoreEvent::Halted,
        };
        core.load_content(&[1]).unwrap();
        let mut session = EmulationSession::new(Box::new(core));
        session.load_content(&[1]).unwrap();
        assert_eq!(session.run_until_event().unwrap(), CoreEvent::Halted);
        assert_eq!(session.state(), SessionState::Halted);
        assert!(session.run_until_event().is_err());
        assert!(session
            .send_input(InputEvent {
                control: crate::core_io::InputControl::Action1,
                pressed: true,
            })
            .is_err());
        session.reset().unwrap();
        assert_eq!(session.state(), SessionState::Running);
    }

    #[test]
    fn media_cannot_be_retrieved_before_content_is_loaded() {
        let mut session = session();
        assert!(session.take_video_frame().is_err());
        assert!(session.take_audio_buffer().is_err());
        session.load_content(&[1]).unwrap();
        assert!(matches!(
            session.take_video_frame(),
            Err(SessionError::Core(CoreError::Unsupported("video output")))
        ));
    }
}


/// Errors returned while selecting or creating a core for a runtime session.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum RuntimeError {
    /// The requested core could not be found or constructed correctly.
    Registry(crate::RegistryError),
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Registry(error) => fmt::Display::fmt(error, formatter),
        }
    }
}

impl std::error::Error for RuntimeError {}

impl From<crate::RegistryError> for RuntimeError {
    fn from(error: crate::RegistryError) -> Self {
        Self::Registry(error)
    }
}

/// Shared, console-independent entry point for discovering cores and opening sessions.
///
/// The runtime owns the registry, not active sessions. Each call to
/// `create_session` constructs a fresh core, so separate sessions never
/// accidentally share mutable machine state.
#[derive(Default)]
pub struct EmulationRuntime {
    registry: crate::CoreRegistry,
}

impl EmulationRuntime {
    /// Creates an empty runtime with no installed core factories.
    pub const fn new() -> Self {
        Self {
            registry: crate::CoreRegistry::new(),
        }
    }

    /// Registers a core factory under its stable system identifier.
    pub fn register_core(
        &mut self,
        id: crate::SystemId,
        factory: crate::CoreFactory,
    ) -> Result<(), crate::RegistryError> {
        self.registry.register(id, factory)
    }

    /// Returns registered system identifiers in stable registration order.
    pub fn available_systems(&self) -> impl Iterator<Item = crate::SystemId> + '_ {
        self.registry.system_ids()
    }

    /// Creates a fresh, initially empty session for a registered system.
    pub fn create_session(
        &self,
        id: crate::SystemId,
    ) -> Result<EmulationSession, RuntimeError> {
        Ok(EmulationSession::new(self.registry.create(id)?))
    }
}

#[cfg(test)]
mod runtime_manager_tests {
    use super::*;
    use crate::{
        CoreCapabilities, CoreDescriptor, CoreError, CoreEvent, EmulationCore, SystemId,
    };

    struct ManagerTestCore;

    impl EmulationCore for ManagerTestCore {
        fn descriptor(&self) -> CoreDescriptor {
            CoreDescriptor {
                system_id: SystemId::new("manager-test").unwrap(),
                display_name: "Manager test core",
                core_version: "0.0.0-test",
                capabilities: CoreCapabilities::default(),
            }
        }

        fn load_content(&mut self, _: &[u8]) -> Result<(), CoreError> {
            Ok(())
        }

        fn reset(&mut self) -> Result<(), CoreError> {
            Ok(())
        }

        fn run_until_event(&mut self) -> Result<CoreEvent, CoreError> {
            Ok(CoreEvent::Yielded)
        }
    }

    fn manager_test_factory() -> Box<dyn EmulationCore> {
        Box::new(ManagerTestCore)
    }

    #[test]
    fn runtime_lists_registered_systems_and_opens_fresh_sessions() {
        let id = SystemId::new("manager-test").unwrap();
        let mut runtime = EmulationRuntime::new();
        assert_eq!(runtime.available_systems().count(), 0);
        runtime.register_core(id, manager_test_factory).unwrap();

        assert_eq!(runtime.available_systems().collect::<Vec<_>>(), vec![id]);
        let first = runtime.create_session(id).unwrap();
        let second = runtime.create_session(id).unwrap();
        assert_eq!(first.state(), SessionState::Empty);
        assert_eq!(second.state(), SessionState::Empty);
        assert_eq!(first.descriptor().system_id, id);
        assert_eq!(second.descriptor().system_id, id);
    }

    #[test]
    fn runtime_reports_unknown_system_without_creating_a_session() {
        let runtime = EmulationRuntime::new();
        let id = SystemId::new("missing").unwrap();
        assert_eq!(
            runtime.create_session(id).unwrap_err(),
            RuntimeError::Registry(crate::RegistryError::UnknownSystem(id))
        );
    }
}
