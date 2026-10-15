//! game page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_game(&mut self, ui: &mut egui::Ui) {
        ui.horizontal_wrapped(|ui| {
            ui.vertical(|ui| {
                ui.heading(if self.language == "fr" { "Moniteur de session" } else { "Session monitor" });
                ui.label("Inspect session lifecycle and media capabilities. This build does not yet provide playable Game Boy emulation.");
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button(if self.language == "fr" { "Gérer les sessions" } else { "Manage sessions" }).clicked() {
                    self.navigate_to(DesktopPage::Sessions);
                }
                if ui.button(if self.language == "fr" { "Ouvrir la bibliothèque" } else { "Open library" }).clicked() {
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
                        if state == SessionState::Running && ui.button(if self.language == "fr" { "Mettre en pause" } else { "Pause" }).clicked() {
                            self.session_action(id, SessionAction::Pause);
                        } else if state == SessionState::Paused && ui.button(if self.language == "fr" { "Reprendre" } else { "Resume" }).clicked() {
                            self.session_action(id, SessionAction::Resume);
                        }
                        if state != SessionState::Empty && ui.button(if self.language == "fr" { "Arrêter" } else { "Stop" }).clicked() {
                            self.session_action(id, SessionAction::Stop);
                        }
                        if state != SessionState::Empty && ui.button(if self.language == "fr" { "Réinitialiser" } else { "Reset" }).clicked() {
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
                    ui.label(if self.language == "fr" { "Entrée : clavier activé si pris en charge" } else { "Input: keyboard mapping enabled when supported" });
                });
            });
        } else {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.heading(if self.language == "fr" { "Aucune session de jeu active" } else { "No active game session" });
                ui.label(if self.language == "fr" { "Importez un fichier .gb ou .gbc et ouvrez une session pour tester le prototype actuel." } else { "Import a local .gb or .gbc file and open a session to inspect the current prototype workflow." });
                if ui.button(if self.language == "fr" { "Ouvrir la bibliothèque" } else { "Browse library" }).clicked() {
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
                    ui.heading(if self.language == "fr" { "Sortie vidéo" } else { "Video output" });
                    ui.label(if self.language == "fr" { "La sortie vidéo attend un cœur capable de produire des images." } else { "Video output is waiting for a core that produces frames." });
                    ui.label(if self.language == "fr" { "Le cœur Game Boy actuel n’implémente pas toute la chaîne CPU/vidéo/audio ; cet espace réservé est donc normal." } else { "The current Game Boy core does not implement the full CPU/video/audio path, so this placeholder is expected." });
                    ui.add_space(12.0);
                    ui.label("↑ ↓ ← →  Move    Z / X  Action    Enter  Start    Backspace  Select");
                    ui.add_space(24.0);
                }
            });
        });
        ui.add_space(6.0);
        ui.horizontal_wrapped(|ui| {
            ui.label(if self.language == "fr" { "État des médias :" } else { "Media status:" });
            ui.label(&self.media_status);
        });
        ui.small("Keyboard controls are forwarded as semantic input transitions only when the core declares input support. Audio plays when the core emits PCM and a host output device is available.");
    }
}
