#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Generic, bounded content envelopes for untrusted game images and firmware.
//!
//! This layer validates only common size and emptiness constraints. Concrete
//! cores remain responsible for validating their own formats and compatibility.

use core::fmt;
use std::io::{ErrorKind, Read};

/// Caller-selected limits applied before content reaches an emulation core.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ContentLimits {
    max_bytes: usize,
}

impl ContentLimits {
    /// Creates limits with an explicit maximum payload size in bytes.
    ///
    /// The maximum is required from the caller because valid content sizes vary
    /// widely between systems. A limit of zero rejects every non-empty payload.
    pub const fn new(max_bytes: usize) -> Self {
        Self { max_bytes }
    }

    /// Returns the configured maximum payload size in bytes.
    pub const fn max_bytes(self) -> usize {
        self.max_bytes
    }
}

/// Common validation failure for a content envelope.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ContentError {
    /// Empty payloads are never accepted as launchable content.
    Empty,
    /// Reading from a content source failed.
    ReadFailed(ErrorKind),
    /// Payload exceeds the explicit caller-provided size limit.
    TooLarge {
        /// Configured maximum payload size in bytes.
        max_bytes: usize,
        /// Actual payload size in bytes.
        actual_bytes: usize,
    },
}

impl fmt::Display for ContentError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Empty => formatter.write_str("content payload cannot be empty"),
            Self::ReadFailed(kind) => write!(formatter, "failed to read content source: {kind}"),
            Self::TooLarge {
                max_bytes,
                actual_bytes,
            } => write!(
                formatter,
                "content payload is too large: maximum {max_bytes} bytes, got {actual_bytes}"
            ),
        }
    }
}

impl std::error::Error for ContentError {}

/// Owned content that has passed common size checks.
///
/// This type does not certify a file format, authenticity, safety of a core,
/// or legal permission to use the bytes. It prevents accidental bypass of the
/// caller's common size policy when using the validated session-loading API.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ContentImage {
    bytes: Vec<u8>,
}

impl ContentImage {
    /// Validates and takes ownership of a non-empty bounded payload.
    pub fn new(bytes: Vec<u8>, limits: ContentLimits) -> Result<Self, ContentError> {
        if bytes.is_empty() {
            return Err(ContentError::Empty);
        }
        if bytes.len() > limits.max_bytes {
            return Err(ContentError::TooLarge {
                max_bytes: limits.max_bytes,
                actual_bytes: bytes.len(),
            });
        }
        Ok(Self { bytes })
    }

    /// Reads at most the configured limit plus one byte from a stream.
    ///
    /// The extra byte detects oversized inputs without reading an unbounded
    /// source into memory. For oversized input, `actual_bytes` reports the
    /// number of bytes observed (at least the configured limit plus one), not
    /// necessarily the source's total length. A stream error is returned as
    /// `ReadFailed`; no partially read payload is exposed.
    pub fn read_from<R: Read>(reader: R, limits: ContentLimits) -> Result<Self, ContentError> {
        let read_limit = limits.max_bytes.saturating_add(1) as u64;
        let mut bytes = Vec::new();
        reader
            .take(read_limit)
            .read_to_end(&mut bytes)
            .map_err(|error| ContentError::ReadFailed(error.kind()))?;
        if bytes.len() > limits.max_bytes {
            return Err(ContentError::TooLarge {
                max_bytes: limits.max_bytes,
                actual_bytes: bytes.len(),
            });
        }
        Self::new(bytes, limits)
    }

    /// Returns the payload bytes for format-specific core validation.
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }

    /// Returns the validated payload size in bytes.
    pub fn len(&self) -> usize {
        self.bytes.len()
    }

    /// Returns whether the payload is empty. Valid instances are never empty.
    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_a_stream_within_the_limit() {
        let image = ContentImage::read_from(&b"abc"[..], ContentLimits::new(3)).unwrap();
        assert_eq!(image.as_bytes(), b"abc");
    }

    #[test]
    fn bounded_reader_stops_after_detecting_oversized_input() {
        let error =
            ContentImage::read_from(&b"abcdef"[..], ContentLimits::new(3)).unwrap_err();
        assert_eq!(
            error,
            ContentError::TooLarge {
                max_bytes: 3,
                actual_bytes: 4,
            }
        );
    }

    #[test]
    fn rejects_empty_streams() {
        assert_eq!(
            ContentImage::read_from(&b""[..], ContentLimits::new(3)),
            Err(ContentError::Empty)
        );
    }

    #[test]
    fn accepts_payload_at_exact_size_limit() {
        let image = ContentImage::new(vec![1, 2, 3], ContentLimits::new(3)).unwrap();
        assert_eq!(image.as_bytes(), &[1, 2, 3]);
        assert_eq!(image.len(), 3);
        assert!(!image.is_empty());
    }

    #[test]
    fn rejects_empty_payloads() {
        assert_eq!(
            ContentImage::new(Vec::new(), ContentLimits::new(10)),
            Err(ContentError::Empty)
        );
    }

    #[test]
    fn rejects_payloads_over_the_explicit_limit() {
        assert_eq!(
            ContentImage::new(vec![1, 2, 3], ContentLimits::new(2)),
            Err(ContentError::TooLarge {
                max_bytes: 2,
                actual_bytes: 3,
            })
        );
    }

    #[test]
    fn zero_limit_rejects_non_empty_payloads() {
        assert!(matches!(
            ContentImage::new(vec![1], ContentLimits::new(0)),
            Err(ContentError::TooLarge {
                max_bytes: 0,
                actual_bytes: 1,
            })
        ));
    }
}
