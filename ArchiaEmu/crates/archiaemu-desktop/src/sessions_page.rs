//! sessions page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_sessions(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.vertical(|ui| {
                ui.heading("Session manager");
                ui.label("Select a session to manage its lifecycle.");
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("Close all").clicked() {
                    let count = self.controller.close_all_sessions();
                    self.frame_cadence.reset();
                    self.frame_texture = None;
                    self.video_size = None;
                    self.last_tick = Instant::now();
                    self.media_status = "No active session.".to_owned();
                    self.message = format!("Closed {count} session(s).");
                }
            });
        });
        ui.add_space(8.0);
        let sessions = self.controller.session_summaries();
        if sessions.is_empty() {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.heading("No open sessions");
                ui.label("Launch a game from your library to create a session.");
                if ui.button("Browse library").clicked() {
                    self.active_page = DesktopPage::Library;
                }
            });
        }
        for summary in sessions {
            let id = summary.id;
            let state = summary.state;
            let content_loaded = summary.content_loaded;
            let system_name = summary.system_name;
            let video_available = summary.video_available;
            let audio_available = summary.audio_available;
            let active_error = summary.last_error;
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal(|ui| {
                    let selected = self.controller.active_session() == Some(id);
                    if ui
                        .selectable_label(selected, format!("Session {}", id.get()))
                        .clicked()
                    {
                        if self.controller.active_session() != Some(id) {
                            self.release_game_inputs();
                            match self.controller.select_session(id) {
                                Ok(()) => {
                                    self.frame_cadence.reset();
                                    self.frame_texture = None;
                                    self.video_size = None;
                                    self.last_tick = Instant::now();
                                    self.active_page = DesktopPage::Game;
                                }
                                Err(error) => self.message = error.to_string(),
                            }
                        }
                    }
                    ui.label(system_name);
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui.button("Close").clicked() {
                            let was_active = self.controller.active_session() == Some(id);
                            match self.controller.close_session(id) {
                                Ok(()) => {
                                    self.message = format!("Closed session {}.", id.get());
                                    if was_active {
                                        self.frame_cadence.reset();
                                        self.frame_texture = None;
                                        self.video_size = None;
                                        self.last_tick = Instant::now();
                                        self.media_status = "Active session closed.".to_owned();
                                    }
                                }
                                Err(error) => self.message = error.to_string(),
                            }
                        }
                    });
                });
                ui.separator();
                ui.horizontal(|ui| {
                    ui.label(format!("State: {state:?}"));
                    ui.separator();
                    ui.label(if content_loaded {
                        "Content: loaded"
                    } else {
                        "Content: not loaded"
                    });
                    if state == SessionState::Running && ui.button("Pause").clicked() {
                        self.session_action(id, SessionAction::Pause);
                    }
                    if state == SessionState::Paused && ui.button("Resume").clicked() {
                        self.session_action(id, SessionAction::Resume);
                    }
                    if state != SessionState::Empty && ui.button("Stop").clicked() {
                        if self.controller.active_session() == Some(id) {
                            self.release_game_inputs();
                            self.frame_cadence.reset();
                            self.frame_texture = None;
                            self.video_size = None;
                            self.media_status = "Active session stopped.".to_owned();
                        }
                        self.session_action(id, SessionAction::Stop);
                    }
                    if state != SessionState::Empty && ui.button("Reset").clicked() {
                        if self.controller.active_session() == Some(id) {
                            self.frame_cadence.reset();
                            self.frame_texture = None;
                            self.video_size = None;
                            self.last_tick = Instant::now();
                            self.media_status = "Active session reset; waiting for a new frame.".to_owned();
                        }
                        self.session_action(id, SessionAction::Reset);
                    }
                });
                ui.horizontal(|ui| {
                    ui.label(if video_available {
                        "Video: available"
                    } else {
                        "Video: unavailable"
                    });
                    ui.separator();
                    ui.label(if audio_available {
                        "Audio: available"
                    } else {
                        "Audio: unavailable"
                    });
                });
                if let Some(error) = active_error {
                    ui.colored_label(
                        egui::Color32::LIGHT_RED,
                        format!("Last core error: {error}"),
                    );
                }
            });
        }
    }
}
