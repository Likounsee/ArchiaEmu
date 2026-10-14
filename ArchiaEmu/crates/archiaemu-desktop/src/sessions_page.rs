//! sessions page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_sessions(&mut self, ui: &mut egui::Ui) {
        ui.horizontal_wrapped(|ui| {
            ui.vertical(|ui| {
                ui.heading("Session manager");
                ui.label("Select a session to manage its lifecycle.");
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("Close all").clicked() {
                    // Keyboard input is routed only to the active session. Release it before
                    // closing sessions so a held control cannot survive the lifecycle transition.
                    let release_error = self.release_game_inputs().err();
                    let count = self.controller.close_all_sessions();
                    self.audio_output.clear();
                    self.frame_cadence.reset();
                    self.frame_texture = None;
                    self.video_size = None;
                    self.last_tick = Instant::now();
                    self.media_status = "No active session.".to_owned();
                    self.last_tick_errors.clear();
                    self.session_content_paths.clear();
                    self.message = match release_error {
                        Some(error) => format!("{error}; closed {count} session(s)."),
                        None => format!("Closed {count} session(s)."),
                    };
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
                    self.navigate_to(DesktopPage::Library);
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
            let active_error = summary.last_error.clone();
            let core_diagnostic = summary.last_error.clone();
            let tick_error = self.last_tick_errors.iter().find_map(|(error_id, error)| {
                if *error_id == id {
                    Some(error.clone())
                } else {
                    None
                }
            });
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal_wrapped(|ui| {
                    let selected = self.controller.active_session() == Some(id);
                    if ui
                        .selectable_label(selected, format!("Session {}", id.get()))
                        .clicked()
                    {
                        if self.controller.active_session() != Some(id) {
                            // Do not switch away if held controls could not be released:
                            // the old session must not keep receiving stuck input.
                            if let Err(error) = self.release_game_inputs() {
                                self.message = error;
                                return;
                            }
                            match self.controller.select_session(id) {
                                Ok(()) => {
                                    self.audio_output.clear();
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
                            let release_error = if was_active {
                                self.release_game_inputs().err()
                            } else {
                                None
                            };
                            match self.controller.close_session(id) {
                                Ok(()) => {
                                    self.message = match release_error {
                                        Some(error) => format!("Session {} closed, but {error}", id.get()),
                                        None => format!("Closed session {}.", id.get()),
                                    };
                                    if was_active {
                                        self.audio_output.clear();
                                        self.frame_cadence.reset();
                                        self.frame_texture = None;
                                        self.video_size = None;
                                        self.last_tick = Instant::now();
                                        self.media_status = "Active session closed.".to_owned();
                                    }
                                    self.last_tick_errors
                                        .retain(|(error_id, _)| *error_id != id);
                                    self.session_content_paths
                                        .retain(|(session_id, _)| *session_id != id);
                                }
                                Err(error) => self.message = error.to_string(),
                            }
                        }
                    });
                });
                if let Some((_, content_path)) = self
                    .session_content_paths
                    .iter()
                    .find(|(session_id, _)| *session_id == id)
                {
                    ui.label(format!("Source: {}", content_path.display()));
                } else if content_loaded {
                    ui.label("Source: path unavailable for this session");
                }
                ui.separator();
                ui.horizontal_wrapped(|ui| {
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
                        self.session_action(id, SessionAction::Stop);
                    }
                    if state != SessionState::Empty && ui.button("Reset").clicked() {
                        self.session_action(id, SessionAction::Reset);
                    }
                });
                ui.horizontal_wrapped(|ui| {
                    ui.label(if video_available {
                        "Video: available"
                    } else {
                        "Video: unavailable"
                    });
                    ui.separator();
                    ui.label(if audio_available {
                        "Audio core output: declared"
                    } else {
                        "Audio core output: unavailable"
                    });
                });
                if let Some(error) = tick_error {
                    ui.colored_label(
                        egui::Color32::LIGHT_RED,
                        format!("Last execution/media tick error: {error}"),
                    );
                }
                if let Some(error) = active_error {
                    ui.colored_label(
                        egui::Color32::LIGHT_RED,
                        format!("Last core error: {error}"),
                    );
                }
                egui::CollapsingHeader::new("Session diagnostics")
                    .id_source(("session_diagnostics", id))
                    .default_open(false)
                    .show(ui, |ui| {
                        ui.label(format!("Session identifier: {}", id.get()));
                        ui.label(format!(
                            "Selected by host: {}",
                            self.controller.active_session() == Some(id)
                        ));
                        ui.label(format!("Lifecycle state: {state:?}"));
                        ui.label(format!("Content loaded: {content_loaded}"));
                        ui.label(format!(
                            "Video output capability: {}",
                            if video_available {
                                "declared available"
                            } else {
                                "unavailable in this core"
                            }
                        ));
                        ui.label(format!(
                            "Audio output capability: {}",
                            if audio_available {
                                "declared available"
                            } else {
                                "unavailable in this core"
                            }
                        ));
                        if !video_available {
                            ui.label("Video output is unavailable because this core does not declare the capability.");
                        }
                        if !audio_available {
                            ui.label("Audio output is unavailable because this core does not declare the capability.");
                        }
                        if let Some(error) = &tick_error {
                            ui.colored_label(
                                egui::Color32::LIGHT_RED,
                                format!("Most recent tick/media failure: {error}"),
                            );
                        } else {
                            ui.label("Most recent tick/media failure: none recorded");
                        }
                        if let Some(error) = &core_diagnostic {
                            ui.colored_label(
                                egui::Color32::LIGHT_RED,
                                format!("Core diagnostic: {error}"),
                            );
                        } else {
                            ui.label("Core diagnostic: none recorded");
                        }
                    });
            });
        }
    }
}
