//! game page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_game(&mut self, ui: &mut egui::Ui) {
        ui.horizontal_wrapped(|ui| {
            ui.vertical(|ui| {
                ui.heading("Game view");
                ui.label("The host runs the selected session and displays its latest video frame.");
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("Manage sessions").clicked() {
                    self.navigate_to(DesktopPage::Sessions);
                }
                if ui.button("Open library").clicked() {
                    self.navigate_to(DesktopPage::Library);
                }
            });
        });
        ui.add_space(8.0);

        let active = self.controller.active_session();
        let session_summary = active.and_then(|id| self.controller.session_summary(id).ok());
        if let Some(summary) = session_summary {
            let id = summary.id;
            let state = summary.state;
            let system = summary.system_name;
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal_wrapped(|ui| {
                    ui.vertical(|ui| {
                        ui.heading(format!("{system} · Session {}", id.get()));
                        ui.label(format!("State: {state:?}"));
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if state == SessionState::Running && ui.button("Pause").clicked() {
                            self.session_action(id, SessionAction::Pause);
                        } else if state == SessionState::Paused && ui.button("Resume").clicked() {
                            self.session_action(id, SessionAction::Resume);
                        }
                        if state != SessionState::Empty && ui.button("Stop").clicked() {
                            self.session_action(id, SessionAction::Stop);
                        }
                        if state != SessionState::Empty && ui.button("Reset").clicked() {
                            self.session_action(id, SessionAction::Reset);
                        }
                    });
                });
                ui.separator();
                ui.horizontal_wrapped(|ui| {
                    ui.label(if summary.video_available {
                        "Video: available"
                    } else {
                        "Video: not implemented by this core"
                    });
                    ui.separator();
                    ui.label(if summary.audio_available {
                        "Audio: core output declared; host playback integrated"
                    } else {
                        "Audio: unavailable"
                    });
                    ui.separator();
                    ui.label("Input: keyboard mapping enabled when supported");
                });
            });
        } else {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.heading("No active game session");
                ui.label("Import a ROM in the library and launch it to create a session.");
                if ui.button("Browse library").clicked() {
                    self.navigate_to(DesktopPage::Library);
                }
            });
        }

        ui.add_space(8.0);
        let available = ui.available_size();
        let display_height = (available.y - 100.0).max(180.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.set_min_height(display_height);
            ui.set_min_width(ui.available_width());
            ui.vertical_centered(|ui| {
                if let (Some(texture), Some([width, height])) = (&self.frame_texture, self.video_size) {
                    let available = ui.available_size();
                    let max_width = available.x.max(1.0);
                    let max_height = available.y.max(1.0);
                    let scale = (max_width / width as f32).min(max_height / height as f32);
                    let size = egui::vec2(width as f32 * scale, height as f32 * scale);
                    ui.image((texture.id(), size));
                    ui.small(format!("{width} × {height} · nearest-neighbour scaling"));
                } else {
                    ui.add_space(24.0);
                    ui.heading("Video output");
                    ui.label("The display surface is ready for core-provided RGBA frames.");
                    ui.label("If the selected core has no video implementation, this area remains a truthful placeholder.");
                    ui.add_space(12.0);
                    ui.label("↑ ↓ ← →  Move    Z / X  Action    Enter  Start    Backspace  Select");
                    ui.add_space(24.0);
                }
            });
        });
        ui.add_space(6.0);
        ui.horizontal_wrapped(|ui| {
            ui.label("Media status:");
            ui.label(&self.media_status);
        });
        ui.small("Keyboard controls are forwarded as semantic input transitions only when the core declares input support. Audio playback is not implemented in this lot.");
    }
}
