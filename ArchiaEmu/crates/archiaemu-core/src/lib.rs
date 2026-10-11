#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Platform-neutral contracts shared by ArchiaEmu and its emulation cores.
//!
//! This crate deliberately contains no console implementation. A system is not
//! supported merely because it can be represented by these types.

use core::fmt;

/// Stable machine-readable identifier for an emulated system.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct SystemId(&'static str);

impl SystemId {
    /// Creates an identifier after validating its syntax.
    ///
    /// Identifiers are lowercase ASCII words separated by single hyphens,
    /// for example `"example-console"`.
    pub const fn new(value: &'static str) -> Result<Self, IdentifierError> {
        let bytes = value.as_bytes();
        if bytes.is_empty() {
            return Err(IdentifierError::Empty);
        }

        let mut index = 0;
        let mut previous_hyphen = true;
        while index < bytes.len() {
            let byte = bytes[index];
            let is_lower = byte >= b'a' && byte <= b'z';
            let is_digit = byte >= b'0' && byte <= b'9';
            let is_hyphen = byte == b'-';
            if !(is_lower || is_digit || is_hyphen) {
                return Err(IdentifierError::InvalidCharacter);
            }
            if is_hyphen && previous_hyphen {
                return Err(IdentifierError::InvalidSeparator);
            }
            previous_hyphen = is_hyphen;
            index += 1;
        }

        if previous_hyphen {
            return Err(IdentifierError::InvalidSeparator);
        }
        Ok(Self(value))
    }

    /// Returns the canonical identifier string.
    pub const fn as_str(self) -> &'static str {
        self.0
    }
}

/// Failure while validating a system identifier.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IdentifierError {
    /// The identifier is empty.
    Empty,
    /// A character other than lowercase ASCII letters, digits, or hyphens was used.
    InvalidCharacter,
    /// A hyphen appears at the beginning/end or two hyphens are adjacent.
    InvalidSeparator,
}

impl fmt::Display for IdentifierError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Empty => formatter.write_str("system identifier cannot be empty"),
            Self::InvalidCharacter => formatter.write_str(
                "system identifier must contain only lowercase ASCII letters, digits, and hyphens",
            ),
            Self::InvalidSeparator => formatter.write_str(
                "system identifier cannot start or end with a hyphen or contain adjacent hyphens",
            ),
        }
    }
}

impl std::error::Error for IdentifierError {}

/// Features a core actually implements. These are declarations, not inferred
/// from the console name or from whether the crate compiles.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct CoreCapabilities {
    /// Core can load content into a fresh or reset machine.
    pub content_loading: bool,
    /// Core supports persistent battery-backed or equivalent data.
    pub persistent_saves: bool,
    /// Core can serialize and restore a complete deterministic machine state.
    pub save_states: bool,
    /// Core can emit video frames.
    pub video_output: bool,
    /// Core can emit audio samples.
    pub audio_output: bool,
    /// Core supports input events.
    pub input: bool,
}

/// Human- and machine-readable metadata for one concrete core.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CoreDescriptor {
    /// Stable identifier used in configuration and diagnostics.
    pub system_id: SystemId,
    /// Display name shown to users.
    pub display_name: &'static str,
    /// Version of this core implementation.
    pub core_version: &'static str,
    /// Features implemented by this core.
    pub capabilities: CoreCapabilities,
}

/// Errors returned when a requested core operation cannot be completed.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CoreError {
    /// Content is malformed, unsupported, or rejected by the core.
    InvalidContent(&'static str),
    /// The operation requires a content image or initialized machine.
    NotInitialized,
    /// The requested capability is not implemented by this core.
    Unsupported(&'static str),
    /// A state file is invalid or incompatible with this core version.
    InvalidState(&'static str),
    /// An execution error with a stable, human-readable explanation.
    Execution(&'static str),
}

impl fmt::Display for CoreError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidContent(reason) => write!(formatter, "invalid content: {reason}"),
            Self::NotInitialized => formatter.write_str("core is not initialized"),
            Self::Unsupported(feature) => write!(formatter, "unsupported capability: {feature}"),
            Self::InvalidState(reason) => write!(formatter, "invalid save state: {reason}"),
            Self::Execution(reason) => write!(formatter, "emulation execution failed: {reason}"),
        }
    }
}

impl std::error::Error for CoreError {}

/// Event emitted when a core reaches an observable boundary.
///
/// Cores may define richer platform-specific event types internally. The common
/// contract intentionally does not force every system into a frame-based model.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CoreEvent {
    /// The core produced a complete video frame.
    FrameReady,
    /// The core produced audio that is ready for the host to consume.
    AudioReady,
    /// The core reached a debugger breakpoint.
    Breakpoint,
    /// The emulated machine halted or stopped executing.
    Halted,
    /// The core reached an implementation-defined scheduling boundary.
    Yielded,
}

/// Lifecycle and execution contract implemented by a concrete emulation core.
pub trait EmulationCore {
    /// Returns immutable metadata for this implementation.
    fn descriptor(&self) -> CoreDescriptor;

    /// Loads content, validating its structure before mutating machine state.
    fn load_content(&mut self, content: &[u8]) -> Result<(), CoreError>;

    /// Resets the initialized machine to its documented reset state.
    fn reset(&mut self) -> Result<(), CoreError>;

    /// Executes until one observable event or scheduling boundary is reached.
    fn run_until_event(&mut self) -> Result<CoreEvent, CoreError>;

    /// Serializes a complete restorable state if save states are supported.
    fn save_state(&self) -> Result<Vec<u8>, CoreError> {
        Err(CoreError::Unsupported("save states"))
    }

    /// Restores a state only after validating its format and compatibility.
    fn load_state(&mut self, _state: &[u8]) -> Result<(), CoreError> {
        Err(CoreError::Unsupported("save states"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepts_canonical_system_ids() {
        assert_eq!(SystemId::new("gb").unwrap().as_str(), "gb");
        assert_eq!(
            SystemId::new("console-64-v2").unwrap().as_str(),
            "console-64-v2"
        );
    }

    #[test]
    fn rejects_empty_or_noncanonical_system_ids() {
        assert_eq!(SystemId::new(""), Err(IdentifierError::Empty));
        assert_eq!(SystemId::new("-gb"), Err(IdentifierError::InvalidSeparator));
        assert_eq!(SystemId::new("gb-"), Err(IdentifierError::InvalidSeparator));
        assert_eq!(
            SystemId::new("gb--color"),
            Err(IdentifierError::InvalidSeparator)
        );
        assert_eq!(
            SystemId::new("GameBoy"),
            Err(IdentifierError::InvalidCharacter)
        );
        assert_eq!(
            SystemId::new("gb_color"),
            Err(IdentifierError::InvalidCharacter)
        );
        assert_eq!(
            SystemId::new("gb/color"),
            Err(IdentifierError::InvalidCharacter)
        );
    }

    #[test]
    fn capabilities_default_to_not_claiming_support() {
        assert_eq!(
            CoreCapabilities::default(),
            CoreCapabilities {
                content_loading: false,
                persistent_saves: false,
                save_states: false,
                video_output: false,
                audio_output: false,
                input: false,
            }
        );
    }

    #[test]
    fn unsupported_save_states_fail_explicitly() {
        struct MetadataOnlyCore;
        impl EmulationCore for MetadataOnlyCore {
            fn descriptor(&self) -> CoreDescriptor {
                CoreDescriptor {
                    system_id: SystemId::new("example").unwrap(),
                    display_name: "Example (contract test only)",
                    core_version: "0.0.0-test",
                    capabilities: CoreCapabilities::default(),
                }
            }
            fn load_content(&mut self, _: &[u8]) -> Result<(), CoreError> {
                Err(CoreError::Unsupported("content loading"))
            }
            fn reset(&mut self) -> Result<(), CoreError> {
                Err(CoreError::NotInitialized)
            }
            fn run_until_event(&mut self) -> Result<CoreEvent, CoreError> {
                Err(CoreError::NotInitialized)
            }
        }

        let core = MetadataOnlyCore;
        assert_eq!(
            core.save_state(),
            Err(CoreError::Unsupported("save states"))
        );
        assert!(!core.descriptor().capabilities.save_states);
    }
}
