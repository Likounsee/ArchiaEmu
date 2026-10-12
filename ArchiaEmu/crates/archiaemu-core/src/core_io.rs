#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Owned, platform-neutral data contracts for core input and media output.
//!
//! These types define data shape and validation only. They do not provide a
//! renderer, audio backend, scheduler, controller mapper, or console emulation.

use core::fmt;

/// Host-level control intent before it is mapped to a console's hardware.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum InputControl {
    /// Up direction.
    Up,
    /// Down direction.
    Down,
    /// Left direction.
    Left,
    /// Right direction.
    Right,
    /// First generic action button.
    Action1,
    /// Second generic action button.
    Action2,
    /// Third generic action button.
    Action3,
    /// Fourth generic action button.
    Action4,
    /// Start or confirm.
    Start,
    /// Select or secondary menu action.
    Select,
}

/// A change in host-level input state.
///
/// Repeated key-down events should be normalized by the host; cores receive
/// state transitions rather than platform-specific keyboard scan codes.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct InputEvent {
    /// Semantic control whose state changed.
    pub control: InputControl,
    /// Whether the control is now held.
    pub pressed: bool,
}

/// Packed pixel representation for a complete video frame.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PixelFormat {
    /// Four bytes per pixel in red, green, blue, alpha order.
    Rgba8888,
}

/// Errors raised when media payload metadata is invalid.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MediaDataError {
    /// Frame dimensions must both be non-zero.
    EmptyFrame,
    /// Dimensions overflow the host address space when calculating byte size.
    FrameSizeOverflow,
    /// Pixel payload length does not match the declared dimensions and format.
    InvalidPixelDataLength {
        /// Expected number of bytes.
        expected: usize,
        /// Actual number of bytes.
        actual: usize,
    },
    /// Audio sample rate or channel count is zero.
    InvalidAudioFormat,
    /// Interleaved sample count is not divisible by the channel count.
    IncompleteAudioFrame {
        /// Number of interleaved samples supplied.
        samples: usize,
        /// Number of channels per sample frame.
        channels: u8,
    },
}

impl fmt::Display for MediaDataError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::EmptyFrame => formatter.write_str("video frame dimensions must be non-zero"),
            Self::FrameSizeOverflow => formatter.write_str("video frame byte size overflows usize"),
            Self::InvalidPixelDataLength { expected, actual } => write!(
                formatter,
                "invalid video pixel data length: expected {expected} bytes, got {actual}"
            ),
            Self::InvalidAudioFormat => {
                formatter.write_str("audio sample rate and channel count must be non-zero")
            }
            Self::IncompleteAudioFrame { samples, channels } => write!(
                formatter,
                "{samples} interleaved audio samples are not divisible by {channels} channels"
            ),
        }
    }
}

impl std::error::Error for MediaDataError {}

/// An owned, complete video frame in a declared pixel format.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VideoFrame {
    /// Frame width in pixels.
    width: u32,
    height: u32,
    format: PixelFormat,
    pixels: Vec<u8>,
}

impl VideoFrame {
    /// Returns the frame width in pixels.
    pub const fn width(&self) -> u32 {
        self.width
    }

    /// Returns the frame height in pixels.
    pub const fn height(&self) -> u32 {
        self.height
    }

    /// Returns the pixel format of this frame.
    pub const fn format(&self) -> PixelFormat {
        self.format
    }

    /// Returns the row-major pixel bytes, with no implicit row padding.
    pub fn pixels(&self) -> &[u8] {
        &self.pixels
    }

    /// Creates a frame after validating dimensions and exact payload length.
    pub fn new(
        width: u32,
        height: u32,
        format: PixelFormat,
        pixels: Vec<u8>,
    ) -> Result<Self, MediaDataError> {
        if width == 0 || height == 0 {
            return Err(MediaDataError::EmptyFrame);
        }
        let bytes_per_pixel = match format {
            PixelFormat::Rgba8888 => 4usize,
        };
        let expected = (width as usize)
            .checked_mul(height as usize)
            .and_then(|pixel_count| pixel_count.checked_mul(bytes_per_pixel))
            .ok_or(MediaDataError::FrameSizeOverflow)?;
        if pixels.len() != expected {
            return Err(MediaDataError::InvalidPixelDataLength {
                expected,
                actual: pixels.len(),
            });
        }
        Ok(Self {
            width,
            height,
            format,
            pixels,
        })
    }
}

/// Owned interleaved signed 16-bit PCM audio samples.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AudioBuffer {
    /// Number of samples per second per channel.
    sample_rate: u32,
    channels: u8,
    samples: Vec<i16>,
}

impl AudioBuffer {
    /// Returns the number of samples per second per channel.
    pub const fn sample_rate(&self) -> u32 {
        self.sample_rate
    }

    /// Returns the number of interleaved channels.
    pub const fn channels(&self) -> u8 {
        self.channels
    }

    /// Returns the interleaved signed 16-bit PCM samples.
    pub fn samples(&self) -> &[i16] {
        &self.samples
    }

    /// Creates an audio buffer with a valid format and complete sample frames.
    pub fn new(sample_rate: u32, channels: u8, samples: Vec<i16>) -> Result<Self, MediaDataError> {
        if sample_rate == 0 || channels == 0 {
            return Err(MediaDataError::InvalidAudioFormat);
        }
        if samples.len() % usize::from(channels) != 0 {
            return Err(MediaDataError::IncompleteAudioFrame {
                samples: samples.len(),
                channels,
            });
        }
        Ok(Self {
            sample_rate,
            channels,
            samples,
        })
    }

    /// Returns the number of complete sample frames in this buffer.
    pub fn frame_count(&self) -> usize {
        self.samples.len() / self.channels as usize
    }
}

/// Canonical host-side input state shared by all emulation cores.
///
/// It deduplicates repeated transitions and can synthesize releases when the
/// host loses focus, preventing a key from remaining stuck in the emulated
/// machine after its physical key-up event was missed.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct InputState {
    pressed: [bool; 10],
}

impl InputState {
    /// Creates an input state with every control released.
    pub const fn new() -> Self {
        Self { pressed: [false; 10] }
    }

    /// Returns whether a semantic control is currently held.
    pub const fn is_pressed(&self, control: InputControl) -> bool {
        self.pressed[control.index()]
    }

    /// Applies a transition and returns whether it changed the held state.
    ///
    /// Repeated key-down or key-up events are ignored and return false.
    pub fn apply(&mut self, event: InputEvent) -> bool {
        let state = &mut self.pressed[event.control.index()];
        if *state == event.pressed {
            return false;
        }
        *state = event.pressed;
        true
    }

    /// Releases all held controls in a stable order and returns their events.
    ///
    /// Hosts should call this when focus is lost or a controller disconnects.
    pub fn release_all(&mut self) -> Vec<InputEvent> {
        let mut released = Vec::new();
        for control in InputControl::ALL {
            if self.apply(InputEvent { control, pressed: false }) {
                released.push(InputEvent { control, pressed: false });
            }
        }
        released
    }

    /// Returns the number of controls currently held.
    pub fn pressed_count(&self) -> usize {
        self.pressed.iter().filter(|pressed| **pressed).count()
    }
}

impl InputControl {
    /// All semantic controls in the canonical release order.
    pub const ALL: [Self; 10] = [
        Self::Up,
        Self::Down,
        Self::Left,
        Self::Right,
        Self::Action1,
        Self::Action2,
        Self::Action3,
        Self::Action4,
        Self::Start,
        Self::Select,
    ];

    const fn index(self) -> usize {
        match self {
            Self::Up => 0,
            Self::Down => 1,
            Self::Left => 2,
            Self::Right => 3,
            Self::Action1 => 4,
            Self::Action2 => 5,
            Self::Action3 => 6,
            Self::Action4 => 7,
            Self::Start => 8,
            Self::Select => 9,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn video_frame_requires_nonzero_dimensions_and_exact_rgba_payload() {
        assert_eq!(
            VideoFrame::new(0, 1, PixelFormat::Rgba8888, vec![]),
            Err(MediaDataError::EmptyFrame)
        );
        assert_eq!(
            VideoFrame::new(2, 1, PixelFormat::Rgba8888, vec![0; 7]),
            Err(MediaDataError::InvalidPixelDataLength {
                expected: 8,
                actual: 7
            })
        );
        let frame = VideoFrame::new(2, 1, PixelFormat::Rgba8888, vec![0; 8]).unwrap();
        assert_eq!(frame.width(), 2);
        assert_eq!(frame.height(), 1);
        assert_eq!(frame.pixels().len(), 8);
    }

    #[test]
    fn audio_buffer_requires_valid_format_and_complete_interleaved_frames() {
        assert_eq!(
            AudioBuffer::new(0, 2, vec![]),
            Err(MediaDataError::InvalidAudioFormat)
        );
        assert_eq!(
            AudioBuffer::new(48_000, 2, vec![1]),
            Err(MediaDataError::IncompleteAudioFrame {
                samples: 1,
                channels: 2
            })
        );
        let audio = AudioBuffer::new(48_000, 2, vec![1, -1, 2, -2]).unwrap();
        assert_eq!(audio.frame_count(), 2);
    }

    #[test]
    fn input_state_deduplicates_repeated_transitions() {
        let mut state = InputState::new();
        let down = InputEvent {
            control: InputControl::Action1,
            pressed: true,
        };
        let up = InputEvent {
            control: InputControl::Action1,
            pressed: false,
        };

        assert!(state.apply(down));
        assert!(!state.apply(down));
        assert!(state.is_pressed(InputControl::Action1));
        assert_eq!(state.pressed_count(), 1);
        assert!(state.apply(up));
        assert!(!state.apply(up));
        assert_eq!(state.pressed_count(), 0);
    }

    #[test]
    fn input_state_releases_all_held_controls_after_focus_loss() {
        let mut state = InputState::new();
        for control in [InputControl::Up, InputControl::Action2, InputControl::Start] {
            assert!(state.apply(InputEvent {
                control,
                pressed: true,
            }));
        }

        let released = state.release_all();
        assert_eq!(released.len(), 3);
        assert!(released.iter().all(|event| !event.pressed));
        assert_eq!(released[0].control, InputControl::Up);
        assert_eq!(released[1].control, InputControl::Action2);
        assert_eq!(released[2].control, InputControl::Start);
        assert_eq!(state.pressed_count(), 0);
        assert!(state.release_all().is_empty());
    }

    #[test]
    fn input_events_are_platform_neutral_state_transitions() {
        assert_eq!(
            InputEvent {
                control: InputControl::Action1,
                pressed: true
            },
            InputEvent {
                control: InputControl::Action1,
                pressed: true
            }
        );
    }
}
