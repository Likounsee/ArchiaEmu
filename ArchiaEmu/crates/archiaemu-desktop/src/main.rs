#![forbid(unsafe_code)]

//! Experimental desktop frontend for the shared ArchiaEmu application controller.

use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};

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
    config_path: Option<PathBuf>,
}

impl DesktopApp {
    fn new() -> Result<Self, Box<dyn Error>> {
        let mut registry = CoreRegistry::new();
        register_game_boy(&mut registry)?;
        let host = HostRuntime::new(HostRuntimeConfig::default())?;
        let mut controller = ApplicationController::new(host);
        let config_path = config_file_path();
        let mut dark_mode = true;
        let mut ui_scale = 1.0;
        let mut message = String::from("Ready. Select a content path to import or launch.");
        if let Some(path) = config_path.as_deref() {
            match load_preferences(path) {
                Ok(Some(saved)) => {
                    dark_mode = saved.dark_mode;
                    ui_scale = saved.ui_scale.clamp(0.8, 1.4);
                    let mut restored = 0usize;
                    for content_path in saved.paths {
                        if controller
                            .import_content(&content_path, MAX_CONTENT_BYTES as u64)
                            .is_ok()
                        {
                            restored += 1;
                        }
                    }
                    message = format!("Restored preferences and {restored} library item(s).");
                }
                Ok(None) => {}
                Err(error) => message = format!("Could not load preferences: {error}"),
            }
        } else {
            message = "Preferences unavailable: no user config directory was found.".to_owned();
        }
        Ok(Self {
            controller,
            registry,
            probe: GameBoyCartridgeProbe,
            content_path: String::new(),
            library_filter: String::new(),
            message,
            dark_mode,
            ui_scale,
            config_path,
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
                self.save_preferences();
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

    fn save_preferences(&mut self) {
        let Some(path) = self.config_path.as_deref() else {
            return;
        };
        let paths: Vec<PathBuf> = self
            .controller
            .library()
            .entries()
            .map(|entry| entry.path().to_path_buf())
            .collect();
        if let Err(error) = write_preferences(path, self.dark_mode, self.ui_scale, &paths) {
            self.message = format!("Could not save preferences: {error}");
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
        let previous_dark_mode = self.dark_mode;
        let previous_ui_scale = self.ui_scale;
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
                        if ui.button("Launch").clicked() {
                            self.content_path = path.display().to_string();
                            self.launch_content();
                        }
                        if ui.button("Forget").clicked() {
                            match self.controller.forget_content(&path) {
                                Ok(_) => {
                                    self.message = format!("Forgot {}", path.display());
                                    self.save_preferences();
                                },
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

        if self.dark_mode != previous_dark_mode
            || (self.ui_scale - previous_ui_scale).abs() > f32::EPSILON
        {
            self.save_preferences();
        }
    }
}

struct SavedPreferences {
    dark_mode: bool,
    ui_scale: f32,
    paths: Vec<PathBuf>,
}

fn config_file_path() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    let base = std::env::var_os("APPDATA").map(PathBuf::from);
    #[cfg(target_os = "macos")]
    let base = std::env::var_os("HOME")
        .map(|home| PathBuf::from(home).join("Library/Application Support"));
    #[cfg(all(unix, not(target_os = "macos")))]
    let base = std::env::var_os("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".config")));
    base.map(|directory| directory.join("ArchiaEmu").join("desktop.conf"))
}

fn load_preferences(path: &Path) -> std::io::Result<Option<SavedPreferences>> {
    let contents = match fs::read_to_string(path) {
        Ok(contents) => contents,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    let mut dark_mode = true;
    let mut ui_scale = 1.0f32;
    let mut paths = Vec::new();
    for line in contents.lines() {
        if let Some(value) = line.strip_prefix("dark=") {
            dark_mode = value == "true";
        } else if let Some(value) = line.strip_prefix("scale=") {
            if let Ok(parsed) = value.parse::<f32>() {
                if parsed.is_finite() {
                    ui_scale = parsed.clamp(0.8, 1.4);
                }
            }
        } else if let Some(value) = line.strip_prefix("path=") {
            if let Some(decoded) = unescape_config_value(value) {
                paths.push(PathBuf::from(decoded));
            }
        }
    }
    Ok(Some(SavedPreferences {
        dark_mode,
        ui_scale,
        paths,
    }))
}

fn write_preferences(
    path: &Path,
    dark_mode: bool,
    ui_scale: f32,
    paths: &[PathBuf],
) -> std::io::Result<()> {
    let parent = path.parent().ok_or_else(|| {
        std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "configuration path has no parent directory",
        )
    })?;
    fs::create_dir_all(parent)?;
    let mut contents = format!("version=1\ndark={dark_mode}\nscale={ui_scale:.3}\n");
    for path in paths {
        contents.push_str("path=");
        contents.push_str(&escape_config_value(&path.to_string_lossy()));
        contents.push('\n');
    }
    let temporary = path.with_extension("conf.tmp");
    fs::write(&temporary, contents)?;
    if path.exists() {
        fs::remove_file(path)?;
    }
    fs::rename(temporary, path)
}

fn escape_config_value(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for character in value.chars() {
        match character {
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            other => escaped.push(other),
        }
    }
    escaped
}

fn unescape_config_value(value: &str) -> Option<String> {
    let mut decoded = String::with_capacity(value.len());
    let mut characters = value.chars();
    while let Some(character) = characters.next() {
        if character != '\\' {
            decoded.push(character);
            continue;
        }
        match characters.next()? {
            '\\' => decoded.push('\\'),
            'n' => decoded.push('\n'),
            'r' => decoded.push('\r'),
            _ => return None,
        }
    }
    Some(decoded)
}

#[cfg(test)]
mod preference_tests {
    use super::*;

    #[test]
    fn config_path_escaping_round_trips_special_characters() {
        let value = "C:\\Games\\my\nrom\r.gb";
        assert_eq!(
            unescape_config_value(&escape_config_value(value)),
            Some(value.to_owned())
        );
    }

    #[test]
    fn malformed_escape_sequences_are_rejected() {
        assert_eq!(unescape_config_value("bad\\x"), None);
        assert_eq!(unescape_config_value("trailing\\"), None);
    }

    #[test]
    fn preference_scale_is_clamped_and_invalid_values_are_ignored() {
        let root = std::env::temp_dir().join(format!("archiaemu-prefs-{}", std::process::id()));
        let file = root.join("desktop.conf");
        write_preferences(&file, false, 2.0, &[PathBuf::from("sample.gb")]).unwrap();
        let loaded = load_preferences(&file).unwrap().unwrap();
        assert!(!loaded.dark_mode);
        assert_eq!(loaded.ui_scale, 1.4);
        assert_eq!(loaded.paths, vec![PathBuf::from("sample.gb")]);
        fs::remove_dir_all(root).unwrap();
    }
}
