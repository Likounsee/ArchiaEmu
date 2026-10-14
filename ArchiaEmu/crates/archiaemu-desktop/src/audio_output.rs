//! Best-effort native audio playback for core-provided signed 16-bit PCM.

use archiaemu_core::core_io::AudioBuffer;
use rodio::{buffer::SamplesBuffer, OutputStream, OutputStreamHandle, Sink};

const MAX_QUEUED_BUFFERS: usize = 8;

/// Owns the native output stream and bounds latency when emulation outpaces playback.
pub(super) struct AudioOutput {
    _stream: Option<OutputStream>,
    sink: Option<Sink>,
    status: String,
    dropped_buffers: u64,
}

impl AudioOutput {
    /// Opens the default host device when available; failure leaves video/emulation usable.
    pub(super) fn new() -> Self {
        match OutputStream::try_default() {
            Ok((stream, handle)) => match Self::create_sink(&handle) {
                Ok(sink) => Self {
                    _stream: Some(stream),
                    sink: Some(sink),
                    status: "Audio device ready".to_owned(),
                    dropped_buffers: 0,
                },
                Err(error) => Self {
                    _stream: Some(stream),
                    sink: None,
                    status: format!("Audio device unavailable: {error}"),
                    dropped_buffers: 0,
                },
            },
            Err(error) => Self {
                _stream: None,
                sink: None,
                status: format!("No audio output device: {error}"),
                dropped_buffers: 0,
            },
        }
    }

    fn create_sink(handle: &OutputStreamHandle) -> Result<Sink, rodio::PlayError> {
        Sink::try_new(handle)
    }

    /// Returns the latest host audio-device status for the UI.
    pub(super) fn status(&self) -> &str {
        &self.status
    }

    /// Enqueues validated PCM for playback; excessive backlog is dropped to cap latency.
    pub(super) fn submit(&mut self, buffer: &AudioBuffer) {
        if buffer.samples().is_empty() {
            return;
        }
        let Some(sink) = self.sink.as_ref() else {
            return;
        };

        if sink.len() >= MAX_QUEUED_BUFFERS {
            sink.clear();
            self.dropped_buffers = self.dropped_buffers.saturating_add(1);
            self.status = format!(
                "Audio active; cleared playback backlog ({} time(s))",
                self.dropped_buffers
            );
        } else if self.dropped_buffers == 0 {
            self.status = "Audio playback active".to_owned();
        }

        let samples: Vec<f32> = buffer
            .samples()
            .iter()
            .map(|sample| f32::from(*sample) / 32768.0)
            .collect();
        if let Some(sink) = self.sink.as_ref() {
            sink.append(SamplesBuffer::new(
                u16::from(buffer.channels()),
                buffer.sample_rate(),
                samples,
            ));
        }
    }

    /// Pauses queued playback while the active session is paused.
    pub(super) fn pause(&self) {
        if let Some(sink) = &self.sink {
            sink.pause();
        }
    }

    /// Resumes queued playback after a session resumes.
    pub(super) fn resume(&self) {
        if let Some(sink) = &self.sink {
            sink.play();
        }
    }

    /// Drops queued samples on stop/reset so stale audio cannot leak into a new run.
    pub(super) fn clear(&self) {
        if let Some(sink) = &self.sink {
            sink.clear();
            sink.play();
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn signed_pcm_normalization_maps_extremes_and_zero() {
        let normalize = |sample: i16| f32::from(sample) / 32768.0;
        assert_eq!(normalize(i16::MIN), -1.0);
        assert_eq!(normalize(0), 0.0);
        assert!(normalize(i16::MAX) < 1.0);
        assert!(normalize(i16::MAX) > 0.99);
    }
}
