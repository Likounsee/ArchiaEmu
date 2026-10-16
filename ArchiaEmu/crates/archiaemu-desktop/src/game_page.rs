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
            let content_loaded = summary.content_loaded;
            let save_states = summary.save_states;
            let system = summary.system_name;
            egui::Frame::group(ui.style())
                .rounding(egui::Rounding::same(12.0))
                .inner_margin(egui::Margin::same(14.0))
                .show(ui, |ui| {
                    ui.horizontal_wrapped(|ui| {
                        ui.vertical(|ui| {
                            ui.heading(format!("{system} · Session {}", id.get()));
                            ui.label(format!("State: {state:?}"));
                        });
                        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                            if state == SessionState::Running
                                && ui
                                    .button(if self.language == "fr" {
                                        "Mettre en pause"
                                    } else {
                                        "Pause"
                                    })
                                    .clicked()
                            {
                                self.session_action(id, SessionAction::Pause);
                            } else if state == SessionState::Paused
                                && ui
                                    .button(if self.language == "fr" {
                                        "Reprendre"
                                    } else {
                                        "Resume"
                                    })
                                    .clicked()
                            {
                                self.session_action(id, SessionAction::Resume);
                            }
                            if content_loaded && save_states {
                                if ui.button(if self.language == "fr" { "Sauvegarder" } else { "Save" }).clicked() {
                                    self.save_state_action(id);
                                }
                                if ui.button(if self.language == "fr" { "Charger l’état" } else { "Load state" }).clicked() {
                                    self.load_state_action(id);
                                }
                                if ui.button(if self.language == "fr" { "Arrêter et sauvegarder" } else { "Stop and Save" }).clicked() {
                                    self.session_action(id, SessionAction::StopAndSave);
                                }
                            }
                            if state != SessionState::Empty
                                && ui
                                    .button(if self.language == "fr" {
                                        "Réinitialiser"
                                    } else {
                                        "Reset"
                                    })
                                    .clicked()
                            {
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
                        ui.label(if summary.persistent_saves {
                            if self.language == "fr" { "Sauvegardes en jeu : disponibles" } else { "In-game saves: available" }
                        } else if self.language == "fr" {
                            "Sauvegardes en jeu : non prises en charge"
                        } else {
                            "In-game saves: not supported by this core"
                        });
                        ui.separator();
                        ui.label(if self.language == "fr" {
                            "Entrée : clavier activé si pris en charge"
                        } else {
                            "Input: keyboard mapping enabled when supported"
                        });
                    });
                });
        } else {
            egui::Frame::group(ui.style())
                .rounding(egui::Rounding::same(12.0))
                .inner_margin(egui::Margin::same(14.0))
                .show(ui, |ui| {
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
        egui::Frame::group(ui.style())
                .rounding(egui::Rounding::same(12.0))
                .inner_margin(egui::Margin::same(14.0))
                .show(ui, |ui| {
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
                    ui.small(format!("{width} x {height} - nearest-neighbour scaling"));
                } else {
                    ui.add_space(14.0);
                    ui.columns(2, |columns| {
                        columns[0].vertical_centered(|ui| {
                            ui.add_space(10.0);
                            let width = ui.available_width().min(270.0);
                            let (rect, _) = ui.allocate_exact_size(
                                egui::vec2(width, 208.0),
                                egui::Sense::hover(),
                            );
                            let accent = self.accent_color;
                            let shell = egui::Rect::from_min_size(
                                rect.min + egui::vec2(width * 0.12, 4.0),
                                egui::vec2(width * 0.76, 194.0),
                            );
                            ui.painter().rect_filled(
                                shell,
                                egui::Rounding::same(24.0),
                                if self.dark_mode {
                                    egui::Color32::from_rgb(56, 65, 86)
                                } else {
                                    egui::Color32::from_rgb(207, 216, 237)
                                },
                            );
                            let screen = egui::Rect::from_min_size(
                                shell.min + egui::vec2(18.0, 18.0),
                                egui::vec2(shell.width() - 36.0, 108.0),
                            );
                            ui.painter().rect_filled(
                                screen,
                                egui::Rounding::same(9.0),
                                if self.dark_mode {
                                    egui::Color32::from_rgb(15, 21, 33)
                                } else {
                                    egui::Color32::from_rgb(237, 244, 255)
                                },
                            );
                            ui.painter().text(
                                screen.center(),
                                egui::Align2::CENTER_CENTER,
                                "ARCHIA",
                                egui::FontId::proportional(18.0),
                                accent,
                            );
                            ui.painter().circle_filled(
                                egui::pos2(shell.min.x + 32.0, shell.min.y + 158.0),
                                8.0,
                                accent,
                            );
                            ui.painter().circle_filled(
                                egui::pos2(shell.min.x + 52.0, shell.min.y + 158.0),
                                8.0,
                                accent.gamma_multiply(0.65),
                            );
                            ui.painter().circle_filled(
                                egui::pos2(shell.max.x - 38.0, shell.min.y + 156.0),
                                10.0,
                                egui::Color32::from_rgb(218, 91, 113),
                            );
                            ui.painter().circle_filled(
                                egui::pos2(shell.max.x - 62.0, shell.min.y + 170.0),
                                10.0,
                                egui::Color32::from_rgb(83, 171, 137),
                            );
                            ui.add_space(10.0);
                            ui.label(egui::RichText::new(if self.language == "fr" {
                                "APERÇU DU MONITEUR"
                            } else {
                                "MONITOR PREVIEW"
                            }).size(10.0).strong().color(ui.visuals().weak_text_color()));
                        });
                        columns[1].vertical(|ui| {
                            ui.add_space(14.0);
                            ui.label(egui::RichText::new(if self.language == "fr" {
                                "EN ATTENTE DU CŒUR"
                            } else {
                                "WAITING FOR CORE"
                            }).size(10.0).strong().color(self.accent_color));
                            ui.add_space(6.0);
                            ui.heading(if self.language == "fr" {
                                "Votre écran de jeu apparaîtra ici"
                            } else {
                                "Your game display will appear here"
                            });
                            ui.add_space(6.0);
                            ui.label(if self.language == "fr" {
                                "Le cœur Game Boy actuel n’émet pas encore les images nécessaires à une session jouable."
                            } else {
                                "The current Game Boy core does not yet emit the frames needed for playable sessions."
                            });
                            ui.add_space(14.0);
                            egui::Frame::group(ui.style())
                                .rounding(egui::Rounding::same(9.0))
                                .inner_margin(egui::Margin::same(12.0))
                                .show(ui, |ui| {
                                    ui.label(egui::RichText::new(if self.language == "fr" {
                                        "COMMANDES PRÉVUES"
                                    } else {
                                        "PLANNED CONTROLS"
                                    }).size(10.0).strong());
                                    ui.add_space(8.0);
                                    ui.horizontal_wrapped(|ui| {
                                        for (key, action) in [
                                            (if self.language == "fr" { "Flèches directionnelles" } else { "Arrow keys" }, if self.language == "fr" { "Déplacement" } else { "Move" }),
                                            ("Z / X", "Action"),
                                            ("Entrée", "Start"),
                                            ("Retour", "Select"),
                                        ] {
                                            ui.label(egui::RichText::new(key).strong());
                                            ui.label(action);
                                            ui.separator();
                                        }
                                    });
                                });
                        });
                    });
                    ui.add_space(14.0);
                }
            });
        });
        ui.add_space(6.0);
        ui.horizontal_wrapped(|ui| {
            ui.label(if self.language == "fr" {
                "État des médias :"
            } else {
                "Media status:"
            });
            ui.label(&self.media_status);
        });
        ui.small("Keyboard controls are forwarded as semantic input transitions only when the core declares input support. Audio plays when the core emits PCM and a host output device is available.");
    }
}
