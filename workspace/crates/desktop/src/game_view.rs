//! Console-agnostic input mapping and video presentation helpers for the desktop frontend.
//!
//! This module adapts the shared runtime contracts to egui. It deliberately does not
//! own sessions, schedule execution, or assume that any particular core emits media.

use eframe::egui;
use emulation_core::core_io::{InputControl, PixelFormat, VideoFrame};

/// Converts a host keyboard key into a semantic runtime input control.
///
/// Platform-specific keyboard events are normalized here; the runtime never sees
/// egui key codes. Unsupported keys are ignored.
pub(crate) fn map_key_to_control(key: egui::Key) -> Option<InputControl> {
    match key {
        egui::Key::ArrowUp => Some(InputControl::Up),
        egui::Key::ArrowDown => Some(InputControl::Down),
        egui::Key::ArrowLeft => Some(InputControl::Left),
        egui::Key::ArrowRight => Some(InputControl::Right),
        egui::Key::Z => Some(InputControl::Action1),
        egui::Key::X => Some(InputControl::Action2),
        egui::Key::A => Some(InputControl::Action3),
        egui::Key::S => Some(InputControl::Action4),
        egui::Key::Enter => Some(InputControl::Start),
        egui::Key::Backspace => Some(InputControl::Select),
        _ => None,
    }
}

/// Converts a validated runtime video frame into a packed egui image.
///
/// The function respects the frame's row stride, rejects unsupported formats,
/// and bounds the packed upload to 64 MiB before allocating the destination buffer.
pub(crate) fn video_frame_to_image(frame: &VideoFrame) -> Result<egui::ColorImage, String> {
    if frame.width() == 0 || frame.height() == 0 {
        return Err("Video frame dimensions must be non-zero.".to_owned());
    }
    if frame.format() != PixelFormat::Rgba8888 {
        return Err("Unsupported video pixel format.".to_owned());
    }
    let width = usize::try_from(frame.width()).map_err(|_| "Video width is too large.")?;
    let height = usize::try_from(frame.height()).map_err(|_| "Video height is too large.")?;
    let packed_row = width.checked_mul(4).ok_or("Video row size overflow.")?;
    let expected = packed_row
        .checked_mul(height)
        .ok_or("Video frame size overflow.")?;
    if expected > 64 * 1024 * 1024 {
        return Err("Video frame exceeds the 64 MiB desktop upload limit.".to_owned());
    }

    let mut packed = Vec::with_capacity(expected);
    for row in frame.pixels().chunks(frame.row_stride_bytes()).take(height) {
        if row.len() < packed_row {
            return Err("Video frame contains a truncated row.".to_owned());
        }
        packed.extend_from_slice(&row[..packed_row]);
    }
    if packed.len() != expected {
        return Err("Video frame row count does not match its dimensions.".to_owned());
    }

    Ok(egui::ColorImage::from_rgba_unmultiplied(
        [width, height],
        &packed,
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keyboard_mapping_is_semantic_and_ignores_unmapped_keys() {
        let expected = [
            (egui::Key::ArrowUp, InputControl::Up),
            (egui::Key::ArrowDown, InputControl::Down),
            (egui::Key::ArrowLeft, InputControl::Left),
            (egui::Key::ArrowRight, InputControl::Right),
            (egui::Key::Z, InputControl::Action1),
            (egui::Key::X, InputControl::Action2),
            (egui::Key::A, InputControl::Action3),
            (egui::Key::S, InputControl::Action4),
            (egui::Key::Enter, InputControl::Start),
            (egui::Key::Backspace, InputControl::Select),
        ];
        for (key, control) in expected {
            assert_eq!(map_key_to_control(key), Some(control));
        }
        assert_eq!(map_key_to_control(egui::Key::Escape), None);
    }

    #[test]
    fn conversion_respects_row_stride_and_preserves_rgba_pixels() {
        let frame = VideoFrame::with_stride(
            1,
            2,
            PixelFormat::Rgba8888,
            8,
            vec![
                255, 0, 0, 255, 99, 99, 99, 99, 0, 255, 0, 255, 88, 88, 88, 88,
            ],
        )
        .unwrap();
        let image = video_frame_to_image(&frame).unwrap();
        assert_eq!(image.size, [1, 2]);
        assert_eq!(image.pixels.len(), 2);
        assert_eq!(
            image.pixels[0],
            egui::Color32::from_rgba_unmultiplied(255, 0, 0, 255)
        );
        assert_eq!(
            image.pixels[1],
            egui::Color32::from_rgba_unmultiplied(0, 255, 0, 255)
        );
    }

    #[test]
    fn conversion_rejects_uploads_over_64_mib() {
        let width = 4096u32;
        let height = 4097u32;
        let frame = VideoFrame::new(
            width,
            height,
            PixelFormat::Rgba8888,
            vec![0; (width as usize) * (height as usize) * 4],
        )
        .unwrap();
        assert!(video_frame_to_image(&frame).unwrap_err().contains("64 MiB"));
    }
}
