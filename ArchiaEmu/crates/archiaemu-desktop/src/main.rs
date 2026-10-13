#![forbid(unsafe_code)]

//! Experimental desktop frontend for the shared ArchiaEmu application controller.

use std::error::Error;
use std::path::Path;

use archiaemu_app::controller::ApplicationController;
use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeConfig, SessionId};
use archiaemu_core::runtime::SessionState;
use archiaemu_core::CoreRegistry;
use archiaemu_game_boy::{register_game_boy, GameBoyCartridgeProbe};
use eframe::egui;

const MAX_CONTENT_BYTES: usize = 32 * 1024 * 1024;

fn main() -> Result<(), Box<dyn Error>> {
    let app = DesktopApp::new()?;
    let options = eframe::NativeOptions::default();
    eframe::run_native(
        "ArchiaEmu",
        options,
        Box::new(move |_creation_context| Box::new(app)),
    )?;
    Ok(())
}

struct DesktopApp {
    controller: ApplicationController,
    registry: CoreRegistry,
    probe: GameBoyCartridgeProbe,
    content_path: String,
    library_filter: String,
    message: String,
    dark_mode: bool,
    ui_scale: f32,
}

impl DesktopApp {
    fn new() -> Result<Self, Box<dyn Error>> {
        let mut registry = CoreRegistry::new();
        register_game_boy(&mut registry)?;
        let host = HostRuntime::new(HostRuntimeConfig::default())?;
        Ok(Self {
            controller: ApplicationController::new(host),
            registry,
            probe: GameBoyCartridgeProbe,
            content_path: String::new(),
            library_filter: String::new(),
            message: String::from("Ready. Select a content path to import or launch."),
            dark_mode: true,
            ui_scale: 1.0,
        })
    }

    fn choose_content_file(&mut self) {
        let selected = rfd::FileDialog::new()
            .add_filter("Game Boy ROM", &["gb", "gbc"])
            .add_filter("All files", &["*"])
            .pick_file();
        if let Some(path) = selected {
            self.content_path = path.display().to_string();
            self.message = format!("Selected {}.", path.display());
        }
    }

    fn import_content(&mut self) {
        let path = self.content_path.trim();
        if path.is_empty() {
            self.message = "Enter a content path first.".to_owned();
            return;
        }
        match self
            .controller
            .import_content(Path::new(path), MAX_CONTENT_BYTES as u64)
        {
            Ok(entry) => {
                self.message = format!(
                    "Imported '{}' ({} bytes).",
                    entry.title(),
                    entry.size_bytes()
                );
            }
            Err(error) => self.message = format!("Import failed: {error}"),
        }
    }

    fn launch_content(&mut self) {
        let path = self.content_path.trim();
        if path.is_empty() {
            self.message = "Enter a content path first.".to_owned();
            return;
        }
        let probes: [&dyn ContentFormatProbe; 1] = [&self.probe];
        match self.controller.launch_content_path(
            Path::new(path),
            ContentLimits::new(MAX_CONTENT_BYTES),
            &probes,
            &self.registry,
        ) {
            Ok(id) => self.message = format!("Launched session {}.", id.get()),
            Err(error) => self.message = format!("Launch failed: {error}"),
        }
    }

    fn session_action(&mut self, id: SessionId, action: SessionAction) {
        let result = self
            .controller
            .host_mut()
            .session_mut(id)
            .map_err(|error| error.to_string())
            .and_then(|session| {
                let operation = match action {
                    SessionAction::Pause => session.pause(),
                    SessionAction::Resume => session.resume(),
                    SessionAction::Stop => session.stop(),
                    SessionAction::Reset => session.reset(),
                };
                operation.map_err(|error| error.to_string())
            });
        self.message = match result {
            Ok(()) => format!("Session {}: {} succeeded.", id.get(), action.label()),
            Err(error) => format!("Session {}: {} failed: {error}", id.get(), action.label()),
        };
    }
}

#[derive(Clone, Copy)]
enum SessionAction {
    Pause,
    Resume,
    Stop,
    Reset,
}

impl SessionAction {
    fn label(self) -> &'static str {
        match self {
            Self::Pause => "pause",
            Self::Resume => "resume",
            Self::Stop => "stop",
            Self::Reset => "reset",
        }
    }
}

impl eframe::App for DesktopApp {
    fn update(&mut self, context: &egui::Context, _frame: &mut eframe::Frame) {
        context.set_visuals(if self.dark_mode {
            egui::Visuals::dark()
        } else {
            egui::Visuals::light()
        });
        context.set_zoom_factor(self.ui_scale);

        egui::TopBottomPanel::top("top").show(context, |ui| {
            ui.heading("ArchiaEmu");
            ui.label(
                "Experimental desktop host — content execution is not yet a playable emulator.",
            );
        });

        egui::CentralPanel::default().show(context, |ui| {
            ui.heading("Content library");
            ui.collapsing("Presentation settings", |ui| {
                ui.checkbox(&mut self.dark_mode, "Dark theme");
                ui.add(
                    egui::Slider::new(&mut self.ui_scale, 0.8..=1.4)
                        .text("Interface scale")
                        .suffix("×"),
                );
                ui.label("These settings affect only the host interface, not emulated hardware.");
            });
            ui.horizontal(|ui| {
                ui.label("Path");
                ui.text_edit_singleline(&mut self.content_path);
                if ui.button("Browse…").clicked() {
                    self.choose_content_file();
                }
                if ui.button("Import").clicked() {
                    self.import_content();
                }
                if ui.button("Launch").clicked() {
                    self.launch_content();
                }
            });

            ui.horizontal(|ui| {
                ui.label("Filter");
                ui.text_edit_singleline(&mut self.library_filter);
                if ui.button("Clear").clicked() {
                    self.library_filter.clear();
                }
            });
            egui::ScrollArea::vertical().max_height(150.0).show(ui, |ui| {
                let query = self.library_filter.trim().to_lowercase();
                let entries: Vec<_> = self
                    .controller
                    .library()
                    .entries()
                    .map(|entry| {
                        (
                            entry.title().to_owned(),
                            entry.path().to_path_buf(),
                            entry.size_bytes(),
                        )
                    })
                    .filter(|(title, path, _)| {
                        query.is_empty()
                            || title.to_lowercase().contains(&query)
                            || path.display().to_string().to_lowercase().contains(&query)
                    })
                    .collect();
                let total = self.controller.library().len();
                if total == 0 {
                    ui.label("No content imported yet.");
                } else if entries.is_empty() {
                    ui.label(format!(
                        "No library entries match ‘{}’.",
                        self.library_filter.trim()
                    ));
                } else {
                    ui.label(format!(
                        "Showing {} of {} item(s).",
                        entries.len(),
                        total
                    ));
                }
                for (title, path, size) in entries {
                    ui.horizontal(|ui| {
                        ui.label(format!("{title} — {size} bytes"));
                        ui.small(path.display().to_string());
                        if ui.button("Use path").clicked() {
                            self.content_path = path.display().to_string();
                        }
                        if ui.button("Forget").clicked() {
                            match self.controller.forget_content(&path) {
                                Ok(_) => self.message = format!("Forgot {}", path.display()),
                                Err(error) => self.message = format!("Could not forget content: {error}"),
                            }
                        }
                    });
                }
            });

            ui.separator();
            ui.heading("Registered systems and capabilities");
            for id in self.registry.system_ids() {
                if let Some(descriptor) = self.registry.registered_descriptor(id) {
                    let caps = descriptor.capabilities;
                    ui.label(format!(
                        "{} — {} | content={} input={} video={} audio={} save_states={} persistent_saves={}",
                        id.as_str(), descriptor.display_name, caps.content_loading, caps.input,
                        caps.video_output, caps.audio_output, caps.save_states, caps.persistent_saves
                    ));
                }
            }

            ui.separator();
            ui.horizontal(|ui| {
                ui.heading("Sessions");
                ui.label(format!(
                    "{}/{} open",
                    self.controller.host().session_count(),
                    self.controller.host().max_sessions()
                ));
                if ui.button("Close all").clicked() {
                    let count = self.controller.close_all_sessions();
                    self.message = format!("Closed {count} session(s).");
                }
            });

            let sessions = self.controller.host().session_ids();
            if sessions.is_empty() {
                ui.label("No sessions running.");
            }
            for id in sessions {
                let (state, system_name, active_error) = match self.controller.host().session(id) {
                    Ok(session) => (
                        session.state(),
                        session.descriptor().display_name.to_owned(),
                        session.last_core_error().map(|error| error.to_string()),
                    ),
                    Err(error) => {
                        self.message = error.to_string();
                        continue;
                    }
                };
                ui.group(|ui| {
                    ui.horizontal(|ui| {
                        let selected = self.controller.active_session() == Some(id);
                        if ui.selectable_label(selected, format!("Session {} — {system_name}", id.get())).clicked() {
                            if let Err(error) = self.controller.select_session(id) {
                                self.message = error.to_string();
                            }
                        }
                        ui.label(format!("{state:?}"));
                        if ui.button("Close").clicked() {
                            match self.controller.close_session(id) {
                                Ok(()) => self.message = format!("Closed session {}.", id.get()),
                                Err(error) => self.message = error.to_string(),
                            }
                        }
                    });
                    ui.horizontal(|ui| {
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
                    if let Some(error) = active_error {
                        ui.colored_label(egui::Color32::LIGHT_RED, format!("Last core error: {error}"));
                    }
                });
            }

            ui.separator();
            ui.label(&self.message);
        });
    }
}
