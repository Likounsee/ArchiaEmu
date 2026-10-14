#![forbid(unsafe_code)]

//! Experimental desktop frontend for the shared ArchiaEmu application controller.

use std::error::Error;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use archiaemu_app::controller::{ApplicationController, SessionCommand};
use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::core_io::{FrameCadence, InputControl, InputEvent};
use game_view::{map_key_to_control, video_frame_to_image};
use preferences::{config_file_path, load_preferences, write_preferences, SavedPreferences};

mod game_view;
mod preferences;
mod game_page;
mod home_page;
mod library_page;
mod sessions_page;
mod systems_page;
mod settings_page;
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeConfig, SessionId};
use archiaemu_core::runtime::SessionState;
use archiaemu_core::CoreRegistry;
use archiaemu_game_boy::{register_game_boy, GameBoyCartridgeProbe};
use eframe::egui;

const MAX_CONTENT_BYTES: usize = 32 * 1024 * 1024;

fn main() -> Result<(), Box<dyn Error>> {
    let app = DesktopApp::new()?;
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1280.0, 820.0])
            .with_min_inner_size([900.0, 600.0]),
        ..Default::default()
    };
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
    library_format: LibraryFormat,
    library_sort: LibrarySort,
    selected_content: Option<PathBuf>,
    message: String,
    dark_mode: bool,
    ui_scale: f32,
    accent_color: egui::Color32,
    config_path: Option<PathBuf>,
    active_page: DesktopPage,
    last_tick: Instant,
    frame_cadence: FrameCadence,
    frame_texture: Option<egui::TextureHandle>,
    video_size: Option<[u32; 2]>,
    media_status: String,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum LibraryFormat {
    All,
    GameBoy,
    GameBoyColor,
}

impl LibraryFormat {
    fn label(self) -> &'static str {
        match self {
            Self::All => "All formats",
            Self::GameBoy => "Game Boy (.gb)",
            Self::GameBoyColor => "Game Boy Color (.gbc)",
        }
    }

    fn matches(self, path: &Path) -> bool {
        let extension = path
            .extension()
            .and_then(|value| value.to_str())
            .unwrap_or("");
        match self {
            Self::All => true,
            Self::GameBoy => extension.eq_ignore_ascii_case("gb"),
            Self::GameBoyColor => extension.eq_ignore_ascii_case("gbc"),
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum LibrarySort {
    Name,
    Largest,
    Path,
}

impl LibrarySort {
    fn label(self) -> &'static str {
        match self {
            Self::Name => "Title (A–Z)",
            Self::Largest => "Largest first",
            Self::Path => "File path",
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum DesktopPage {
    Home,
    Library,
    Sessions,
    Game,
    Systems,
    Settings,
}

impl DesktopPage {
    fn title(self) -> &'static str {
        match self {
            Self::Home => "Overview",
            Self::Library => "Game library",
            Self::Sessions => "Sessions",
            Self::Game => "Game",
            Self::Systems => "Systems",
            Self::Settings => "Settings",
        }
    }
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
        let mut accent_color = egui::Color32::from_rgb(75, 115, 210);
        let mut message = String::from("Ready. Select a content path to import or launch.");
        if let Some(path) = config_path.as_deref() {
            match load_preferences(path) {
                Ok(Some(saved)) => {
                    dark_mode = saved.dark_mode;
                    ui_scale = saved.ui_scale.clamp(0.8, 1.4);
                    accent_color = egui::Color32::from_rgb(saved.accent_rgb[0], saved.accent_rgb[1], saved.accent_rgb[2]);
                    let mut restored = 0usize;
                    let mut skipped = 0usize;
                    let mut skipped_details = Vec::new();
                    for content_path in saved.paths {
                        match controller.import_content(&content_path, MAX_CONTENT_BYTES as u64) {
                            Ok(_) => restored += 1,
                            Err(error) => {
                                skipped += 1;
                                if skipped_details.len() < 3 {
                                    skipped_details.push(format!("{}: {error}", content_path.display()));
                                }
                            }
                        }
                    }
                    message = if skipped == 0 {
                        format!("Restored preferences and {restored} library item(s).")
                    } else {
                        let details = if skipped_details.is_empty() {
                            String::new()
                        } else {
                            format!(" Details: {}", skipped_details.join("; "))
                        };
                        format!(
                            "Restored {restored} library item(s); skipped {skipped} unavailable or invalid path(s). Re-import those files if needed.{details}"
                        )
                    };
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
            library_format: LibraryFormat::All,
            library_sort: LibrarySort::Name,
            selected_content: None,
            message,
            dark_mode,
            ui_scale,
            accent_color,
            config_path,
            active_page: DesktopPage::Home,
            last_tick: Instant::now(),
            frame_cadence: FrameCadence::new(60, 3)?,
            frame_texture: None,
            video_size: None,
            media_status: "No active session.".to_owned(),
        })
    }

    fn choose_content_file(&mut self) {
        let selected = rfd::FileDialog::new()
            .add_filter("Game Boy ROMs", &["gb", "gbc"])
            .add_filter("Game Boy ROM", &["gb"])
            .add_filter("Game Boy Color ROM", &["gbc"])
            .add_filter("All files", &["*"])
            .pick_files();
        if let Some(paths) = selected {
            if paths.is_empty() {
                return;
            }
            let mut imported = 0usize;
            let mut rejected = 0usize;
            let mut first_imported_path = None;
            let mut failures = Vec::new();
            for path in paths {
                match self
                    .controller
                    .import_content(&path, MAX_CONTENT_BYTES as u64)
                {
                    Ok(entry) => {
                        imported += 1;
                        if first_imported_path.is_none() {
                            first_imported_path = Some(entry.path().to_path_buf());
                        }
                    }
                    Err(error) => {
                        rejected += 1;
                        if failures.len() < 3 {
                            failures.push(format!("{}: {error}", path.display()));
                        }
                    }
                }
            }
            if let Some(path) = first_imported_path {
                self.content_path = path.display().to_string();
                self.selected_content = Some(path);
            }
            if imported > 0 {
                self.save_preferences();
                self.active_page = DesktopPage::Library;
            }
            self.message = if rejected == 0 {
                format!("Imported {imported} ROM(s) into your library.")
            } else {
                let detail = if failures.is_empty() {
                    String::new()
                } else {
                    format!(" Details: {}", failures.join("; "))
                };
                format!(
                    "Imported {imported} ROM(s); {rejected} file(s) skipped or rejected.{detail}"
                )
            };
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
                self.selected_content = Some(entry.path().to_path_buf());
                self.message = format!(
                    "Imported '{}' ({}).",
                    entry.title(),
                    format_size(entry.size_bytes())
                );
                self.save_preferences();
                self.active_page = DesktopPage::Library;
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
            Ok(id) => {
                self.message = format!("Launched session {}.", id.get());
                self.frame_cadence.reset();
                self.frame_texture = None;
                self.video_size = None;
                self.last_tick = Instant::now();
                self.active_page = DesktopPage::Game;
            }
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
        if let Err(error) = write_preferences(path, self.dark_mode, self.ui_scale, [self.accent_color.r(), self.accent_color.g(), self.accent_color.b()], &paths) {
            self.message = format!("Could not save preferences: {error}");
        }
    }

    fn session_action(&mut self, id: SessionId, action: SessionAction) {
        let command = match action {
            SessionAction::Pause => SessionCommand::Pause,
            SessionAction::Resume => SessionCommand::Resume,
            SessionAction::Stop => SessionCommand::Stop,
            SessionAction::Reset => SessionCommand::Reset,
        };
        let result = self
            .controller
            .command_session(id, command)
            .map_err(|error| error.to_string());
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
        self.advance_emulation(context);
        let previous_dark_mode = self.dark_mode;
        let previous_ui_scale = self.ui_scale;
        let previous_accent_color = self.accent_color;
        context.set_visuals(if self.dark_mode {
            egui::Visuals::dark()
        } else {
            egui::Visuals::light()
        });
        context.set_zoom_factor(self.ui_scale);
        context.style_mut(|style| {
            style.spacing.item_spacing = egui::vec2(10.0, 10.0);
            style.spacing.button_padding = egui::vec2(12.0, 8.0);
            style.spacing.window_margin = egui::Margin::same(16.0);
            let accent = self.accent_color;
            style.visuals.selection.bg_fill = accent.gamma_multiply(0.45);
            style.visuals.selection.stroke = egui::Stroke::new(1.0_f32, accent);
            style.visuals.widgets.inactive.bg_stroke = egui::Stroke::new(1.0, accent.gamma_multiply(0.45));
            style.visuals.widgets.hovered.bg_fill = accent.gamma_multiply(0.22);
            style.visuals.widgets.hovered.bg_stroke = egui::Stroke::new(1.0, accent);
            style.visuals.widgets.active.bg_fill = accent.gamma_multiply(0.38);
            style.visuals.widgets.active.bg_stroke = egui::Stroke::new(1.0, accent);
            style.visuals.widgets.inactive.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.hovered.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.active.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.noninteractive.rounding = egui::Rounding::same(6.0);
        });

        egui::SidePanel::left("navigation")
            .resizable(false)
            .exact_width(204.0)
            .show(context, |ui| {
                ui.add_space(10.0);
                ui.horizontal(|ui| {
                    ui.heading("◈");
                    ui.vertical(|ui| {
                        ui.heading("ArchiaEmu");
                        ui.small("EMULATION STUDIO");
                    });
                });
                ui.add_space(24.0);
                ui.label("WORKSPACE");
                ui.add_space(4.0);
                for (page, glyph, label) in [
                    (DesktopPage::Home, "⌂", "Overview"),
                    (DesktopPage::Library, "▤", "Game library"),
                    (DesktopPage::Sessions, "▣", "Sessions"),
                    (DesktopPage::Game, "▶", "Game"),
                    (DesktopPage::Systems, "◉", "Systems"),
                    (DesktopPage::Settings, "⚙", "Settings"),
                ] {
                    let selected = self.active_page == page;
                    if ui
                        .selectable_label(selected, format!("{glyph}   {label}"))
                        .clicked()
                    {
                        if self.active_page == DesktopPage::Game && page != DesktopPage::Game {
                            self.release_game_inputs();
                        }
                        self.active_page = page;
                    }
                }
                ui.with_layout(egui::Layout::bottom_up(egui::Align::Min), |ui| {
                    ui.separator();
                    ui.label("EARLY DEVELOPMENT");
                    ui.small("Core support is experimental.");
                    ui.small("No game compatibility is implied.");
                });
            });

        egui::TopBottomPanel::top("app_header").show(context, |ui| {
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.heading(self.active_page.title());
                    ui.label("Your library, sessions and emulator tools in one place.");
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.label(if self.dark_mode {
                        "◐ Dark"
                    } else {
                        "☼ Light"
                    });
                    ui.label(format!(
                        "{} session(s)",
                        self.controller.session_count()
                    ));
                });
            });
        });

        egui::CentralPanel::default().show(context, |ui| {
            match self.active_page {
                DesktopPage::Home => self.show_home(ui),
                DesktopPage::Library => self.show_library(ui),
                DesktopPage::Sessions => self.show_sessions(ui),
                DesktopPage::Game => self.show_game(ui),
                DesktopPage::Systems => self.show_systems(ui),
                DesktopPage::Settings => self.show_settings(ui),
            }
            ui.add_space(12.0);
            ui.separator();
            ui.horizontal(|ui| {
                let is_error = [
                    "Import failed:",
                    "Launch failed:",
                    "Could not ",
                    "Preferences unavailable:",
                    "Session ",
                ]
                .iter()
                .any(|prefix| self.message.starts_with(prefix))
                    && !self.message.ends_with(" succeeded.");
                let (symbol, color) = if is_error {
                    ("●", egui::Color32::LIGHT_RED)
                } else {
                    ("●", egui::Color32::from_rgb(95, 190, 140))
                };
                ui.colored_label(color, symbol);
                ui.label(&self.message);
            });
        });

        if self.dark_mode != previous_dark_mode
            || (self.ui_scale - previous_ui_scale).abs() > f32::EPSILON
            || self.accent_color != previous_accent_color
        {
            self.save_preferences();
        }
    }
}

impl DesktopApp {
    fn advance_emulation(&mut self, context: &egui::Context) {
        let now = Instant::now();
        let elapsed = now.saturating_duration_since(self.last_tick);
        self.last_tick = now;

        if self.active_page == DesktopPage::Game {
            let events = context.input(|input| input.events.clone());
            for event in events {
                match event {
                    egui::Event::Key {
                        key,
                        pressed,
                        repeat,
                        ..
                    } => {
                        if repeat && pressed {
                            continue;
                        }
                        if let Some(control) = map_key_to_control(key) {
                            self.send_game_input(control, pressed);
                        }
                    }
                    egui::Event::WindowFocused(false) => self.release_game_inputs(),
                    _ => {}
                }
            }
        }

        let Some(id) = self.controller.active_session() else {
            self.frame_cadence.reset();
            self.frame_texture = None;
            self.video_size = None;
            self.media_status = "No active session. Launch a ROM to receive core-provided frames.".to_owned();
            return;
        };

        let update = match self.controller.advance_session(id, elapsed, &mut self.frame_cadence) {
            Ok(update) => update,
            Err(_) => {
                self.media_status = "The selected session no longer exists.".to_owned();
                self.frame_texture = None;
                self.video_size = None;
                return;
            }
        };
        let state = update.state;
        let video_available = update.video_available;
        let audio_available = update.audio_available;
        let mut error = update.error;
        let newest_frame = update.newest_frame;

        if let Some(frame) = newest_frame {
            match video_frame_to_image(&frame) {
                Ok(image) => {
                    let [width, height] = image.size;
                    self.video_size = Some([width as u32, height as u32]);
                    if let Some(texture) = self.frame_texture.as_mut() {
                        texture.set(image, egui::TextureOptions::NEAREST);
                    } else {
                        self.frame_texture = Some(context.load_texture(
                            "archiaemu-game-frame",
                            image,
                            egui::TextureOptions::NEAREST,
                        ));
                    }
                }
                Err(frame_error) => error = Some(frame_error),
            }
        }

        self.media_status = if let Some(error) = error {
            error
        } else if state == SessionState::Paused {
            "Paused — video output is frozen until you resume.".to_owned()
        } else if state != SessionState::Running {
            format!("Session state: {state:?}.")
        } else if !video_available {
            "This core does not currently advertise video output; the display area is a placeholder.".to_owned()
        } else if audio_available {
            "Video pipeline connected. Audio is advertised by the core but host playback is not integrated yet.".to_owned()
        } else {
            "Video pipeline connected. This core does not currently advertise audio output."
                .to_owned()
        };
        if state == SessionState::Running {
            context.request_repaint_after(Duration::from_millis(16));
        }
    }

    fn send_game_input(&mut self, control: InputControl, pressed: bool) {
        let Some(id) = self.controller.active_session() else {
            return;
        };
        let result = self
            .controller
            .send_input(id, InputEvent { control, pressed })
            .map_err(|error| error.to_string());
        if let Err(error) = result {
            if !error.contains("unsupported") && !error.contains("input") {
                self.message = format!("Input error: {error}");
            }
        }
    }

    fn release_game_inputs(&mut self) {
        let Some(id) = self.controller.active_session() else {
            return;
        };
        let result = self
            .controller
            .release_inputs(id)
            .map_err(|error| error.to_string());
        if let Err(error) = result {
            self.message = format!("Could not release held inputs: {error}");
        }
    }


}

fn format_size(bytes: u64) -> String {
    const KIB: u64 = 1024;
    const MIB: u64 = KIB * 1024;
    if bytes >= MIB {
        format!("{:.2} MiB", bytes as f64 / MIB as f64)
    } else if bytes >= KIB {
        format!("{:.1} KiB", bytes as f64 / KIB as f64)
    } else {
        format!("{bytes} B")
    }
}

fn capability_label(enabled: bool) -> &'static str {
    if enabled {
        "Available"
    } else {
        "Not implemented"
    }
}

#[cfg(test)]
mod preference_tests {
    use super::*;
    use crate::preferences::{escape_config_value, load_preferences, unescape_config_value, write_preferences};
    use std::fs;

    #[test]
    fn library_format_filter_is_case_insensitive() {
        assert!(LibraryFormat::GameBoy.matches(Path::new("Pokemon.GB")));
        assert!(LibraryFormat::GameBoyColor.matches(Path::new("Pokemon.GbC")));
        assert!(!LibraryFormat::GameBoy.matches(Path::new("Pokemon.gbc")));
        assert!(LibraryFormat::All.matches(Path::new("archive.zip")));
    }

    #[test]
    fn displayed_file_sizes_use_readable_units() {
        assert_eq!(format_size(512), "512 B");
        assert_eq!(format_size(1536), "1.5 KiB");
        assert_eq!(format_size(2 * 1024 * 1024), "2.00 MiB");
    }

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

    #[test]
    fn replacing_preferences_keeps_only_the_new_complete_config() {
        let root =
            std::env::temp_dir().join(format!("archiaemu-prefs-replace-{}", std::process::id()));
        let file = root.join("desktop.conf");
        write_preferences(&file, true, 1.0, &[PathBuf::from("old.gb")]).unwrap();
        write_preferences(&file, false, 1.2, &[PathBuf::from("new.gb")]).unwrap();

        let loaded = load_preferences(&file).unwrap().unwrap();
        assert!(!loaded.dark_mode);
        assert_eq!(loaded.ui_scale, 1.2);
        assert_eq!(loaded.paths, vec![PathBuf::from("new.gb")]);
        assert!(!file.with_extension("conf.tmp").exists());
        assert!(!file.with_extension("conf.bak").exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn invalid_scale_lines_do_not_override_a_valid_scale() {
        let root = std::env::temp_dir().join(format!(
            "archiaemu-prefs-invalid-scale-{}",
            std::process::id()
        ));
        fs::create_dir_all(&root).unwrap();
        let file = root.join("desktop.conf");
        fs::write(&file, "version=1\ndark=false\nscale=NaN\nscale=oops\n").unwrap();

        let loaded = load_preferences(&file).unwrap().unwrap();
        assert!(!loaded.dark_mode);
        assert_eq!(loaded.ui_scale, 1.0);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn unsupported_preferences_version_is_rejected() {
        let root =
            std::env::temp_dir().join(format!("archiaemu-prefs-version-{}", std::process::id()));
        fs::create_dir_all(&root).unwrap();
        let file = root.join("desktop.conf");
        fs::write(&file, "version=99\ndark=false\n").unwrap();

        let error = match load_preferences(&file) {
            Err(error) => error,
            Ok(_) => panic!("unsupported preferences version should be rejected"),
        };
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        fs::remove_dir_all(root).unwrap();
    }
}
