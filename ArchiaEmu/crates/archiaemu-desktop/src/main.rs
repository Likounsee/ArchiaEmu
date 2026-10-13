#![forbid(unsafe_code)]

//! Experimental desktop frontend for the shared ArchiaEmu application controller.

use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use archiaemu_app::controller::ApplicationController;
use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::core_io::{FrameCadence, InputControl, InputEvent, PixelFormat, VideoFrame};
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
            library_format: LibraryFormat::All,
            library_sort: LibrarySort::Name,
            selected_content: None,
            message,
            dark_mode,
            ui_scale,
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
            self.content_path = paths[0].display().to_string();
            let mut imported = 0usize;
            let mut rejected = 0usize;
            let mut failures = Vec::new();
            for path in paths {
                match self
                    .controller
                    .import_content(&path, MAX_CONTENT_BYTES as u64)
                {
                    Ok(_) => imported += 1,
                    Err(error) => {
                        rejected += 1;
                        if failures.len() < 3 {
                            failures.push(format!("{}: {error}", path.display()));
                        }
                    }
                }
            }
            self.selected_content = Some(PathBuf::from(&self.content_path));
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
        self.advance_emulation(context);
        let previous_dark_mode = self.dark_mode;
        let previous_ui_scale = self.ui_scale;
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
            style.visuals.selection.bg_fill = egui::Color32::from_rgb(75, 115, 210);
            style.visuals.selection.stroke =
                egui::Stroke::new(1.0_f32, egui::Color32::from_rgb(125, 160, 245));
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
                        self.controller.host().session_count()
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

        let mut newest_frame: Option<VideoFrame> = None;
        let mut error = None;
        let video_available;
        let audio_available;
        let state;
        {
            let session_result = self.controller.host_mut().session_mut(id);
            let Ok(session) = session_result else {
                self.media_status = "The selected session no longer exists.".to_owned();
                self.frame_texture = None;
                self.video_size = None;
                return;
            };
            state = session.state();
            let capabilities = session.descriptor().capabilities;
            video_available = capabilities.video_output;
            audio_available = capabilities.audio_output;
            let due = session.advance_frame_cadence(&mut self.frame_cadence, elapsed);
            if state == SessionState::Running {
                for _ in 0..due.due_frames {
                    if let Err(run_error) = session.run_for_events(1024) {
                        error = Some(format!("Execution error: {run_error}"));
                        break;
                    }
                    if session.state() != SessionState::Running {
                        break;
                    }
                }
            }
            if video_available && state != SessionState::Empty {
                for _ in 0..4 {
                    match session.take_video_frame() {
                        Ok(Some(frame)) => newest_frame = Some(frame),
                        Ok(None) => break,
                        Err(frame_error) => {
                            error = Some(format!("Video output error: {frame_error}"));
                            break;
                        }
                    }
                }
            }
        }

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
            .host_mut()
            .session_mut(id)
            .map_err(|error| error.to_string())
            .and_then(|session| {
                session
                    .send_input(InputEvent { control, pressed })
                    .map_err(|error| error.to_string())
            });
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
            .host_mut()
            .session_mut(id)
            .map_err(|error| error.to_string())
            .and_then(|session| {
                if matches!(
                    session.state(),
                    SessionState::Running | SessionState::Paused
                ) {
                    session
                        .release_all_inputs()
                        .map_err(|error| error.to_string())
                } else {
                    Ok(())
                }
            });
        if let Err(error) = result {
            self.message = format!("Could not release held inputs: {error}");
        }
    }

    fn show_game(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.vertical(|ui| {
                ui.heading("Game view");
                ui.label("The host runs the selected session and displays its latest video frame.");
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("Manage sessions").clicked() {
                    self.active_page = DesktopPage::Sessions;
                }
                if ui.button("Open library").clicked() {
                    self.active_page = DesktopPage::Library;
                }
            });
        });
        ui.add_space(8.0);

        let active = self.controller.active_session();
        let session_summary = active.and_then(|id| {
            self.controller.host().session(id).ok().map(|session| {
                (
                    id,
                    session.state(),
                    session.descriptor().display_name.to_owned(),
                    session.descriptor().capabilities,
                )
            })
        });
        if let Some((id, state, system, capabilities)) = session_summary {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.vertical(|ui| {
                        ui.heading(format!("{system} · Session {}", id.get()));
                        ui.label(format!("State: {state:?}"));
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if state == SessionState::Running && ui.button("Pause").clicked() {
                            self.session_action(id, SessionAction::Pause);
                            self.frame_cadence.reset();
                        } else if state == SessionState::Paused && ui.button("Resume").clicked() {
                            self.session_action(id, SessionAction::Resume);
                            self.last_tick = Instant::now();
                        }
                        if state != SessionState::Empty && ui.button("Stop").clicked() {
                            self.release_game_inputs();
                            self.session_action(id, SessionAction::Stop);
                            self.frame_cadence.reset();
                            self.frame_texture = None;
                            self.video_size = None;
                        }
                        if state != SessionState::Empty && ui.button("Reset").clicked() {
                            self.session_action(id, SessionAction::Reset);
                            self.frame_cadence.reset();
                            self.frame_texture = None;
                            self.video_size = None;
                            self.last_tick = Instant::now();
                        }
                    });
                });
                ui.separator();
                ui.horizontal(|ui| {
                    ui.label(if capabilities.video_output {
                        "Video: available"
                    } else {
                        "Video: not implemented by this core"
                    });
                    ui.separator();
                    ui.label(if capabilities.audio_output {
                        "Audio: core capability declared"
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
                    self.active_page = DesktopPage::Library;
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

    fn show_home(&mut self, ui: &mut egui::Ui) {
        ui.heading("Welcome back");
        ui.label("Pick up where you left off or add a game to your library.");
        ui.add_space(8.0);

        ui.columns(3, |columns| {
            let cards = [
                (
                    "LIBRARY",
                    self.controller.library().len().to_string(),
                    "Imported items",
                ),
                (
                    "SESSIONS",
                    self.controller.host().session_count().to_string(),
                    "Open sessions",
                ),
                (
                    "SYSTEMS",
                    self.registry.system_ids().count().to_string(),
                    "Registered cores",
                ),
            ];
            for (column, (title, value, detail)) in columns.iter_mut().zip(cards) {
                egui::Frame::group(column.style()).show(column, |ui| {
                    ui.label(title);
                    ui.heading(value);
                    ui.small(detail);
                });
            }
        });

        ui.add_space(14.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.heading("Start playing");
            ui.label("Choose a Game Boy or Game Boy Color ROM to add it to your library.");
            ui.add_space(8.0);
            ui.horizontal(|ui| {
                ui.add(
                    egui::TextEdit::singleline(&mut self.content_path)
                        .hint_text("Path to a .gb or .gbc file")
                        .desired_width(ui.available_width() * 0.58),
                );
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
        });

        ui.add_space(14.0);
        ui.horizontal(|ui| {
            ui.heading("Recently imported");
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.link("View library →").clicked() {
                    self.active_page = DesktopPage::Library;
                }
            });
        });
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
            .collect();
        if entries.is_empty() {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.label("Your library is empty.");
                ui.label("Use Browse or paste a ROM path above to get started.");
            });
        } else {
            egui::ScrollArea::vertical()
                .max_height(210.0)
                .show(ui, |ui| {
                    for (title, path, size) in entries.iter().take(5) {
                        ui.horizontal(|ui| {
                            ui.label("▣");
                            ui.vertical(|ui| {
                                ui.label(title);
                                ui.small(format!("{} · {} bytes", path.display(), size));
                            });
                            ui.with_layout(
                                egui::Layout::right_to_left(egui::Align::Center),
                                |ui| {
                                    if ui.button("Launch").clicked() {
                                        self.content_path = path.display().to_string();
                                        self.launch_content();
                                    }
                                },
                            );
                        });
                        ui.separator();
                    }
                });
        }

        ui.add_space(12.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.heading("Emulation status");
                    ui.label("The desktop shell is being built before full hardware emulation.");
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.label("Experimental");
                });
            });
        });
    }

    fn show_library(&mut self, ui: &mut egui::Ui) {
        let total = self.controller.library().len();
        ui.horizontal(|ui| {
            ui.vertical(|ui| {
                ui.heading("Your games");
                ui.label(format!("{total} imported item(s)"));
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("＋ Import ROMs…").clicked() {
                    self.choose_content_file();
                }
            });
        });

        ui.add_space(8.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.label("⌕ Search");
                ui.add(
                    egui::TextEdit::singleline(&mut self.library_filter)
                        .hint_text("Search titles and file paths…")
                        .desired_width(260.0),
                );
                egui::ComboBox::from_id_source("library_format_filter")
                    .selected_text(self.library_format.label())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::All,
                            "All formats",
                        );
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::GameBoy,
                            "Game Boy (.gb)",
                        );
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::GameBoyColor,
                            "Game Boy Color (.gbc)",
                        );
                    });
                egui::ComboBox::from_id_source("library_sort")
                    .selected_text(self.library_sort.label())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.library_sort,
                            LibrarySort::Name,
                            "Title (A–Z)",
                        );
                        ui.selectable_value(
                            &mut self.library_sort,
                            LibrarySort::Largest,
                            "Largest first",
                        );
                        ui.selectable_value(&mut self.library_sort, LibrarySort::Path, "File path");
                    });
                if ui.button("Clear").clicked() {
                    self.library_filter.clear();
                    self.library_format = LibraryFormat::All;
                }
            });
        });

        let query = self.library_filter.trim().to_lowercase();
        let mut entries: Vec<_> = self
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
                (query.is_empty()
                    || title.to_lowercase().contains(&query)
                    || path.to_string_lossy().to_lowercase().contains(&query))
                    && self.library_format.matches(path)
            })
            .collect();
        match self.library_sort {
            LibrarySort::Name => entries.sort_by(|a, b| {
                a.0.to_lowercase()
                    .cmp(&b.0.to_lowercase())
                    .then_with(|| a.1.cmp(&b.1))
            }),
            LibrarySort::Largest => entries.sort_by(|a, b| {
                b.2.cmp(&a.2)
                    .then_with(|| a.0.to_lowercase().cmp(&b.0.to_lowercase()))
            }),
            LibrarySort::Path => entries.sort_by(|a, b| a.1.cmp(&b.1)),
        }

        ui.add_space(6.0);
        ui.horizontal(|ui| {
            ui.label(format!("Showing {} of {total}", entries.len()));
            ui.separator();
            let total_size: u64 = self
                .controller
                .library()
                .entries()
                .map(|entry| entry.size_bytes())
                .sum();
            ui.label(format!("{} total size", format_size(total_size)));
        });
        ui.add_space(6.0);

        if entries.is_empty() {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.add_space(8.0);
                ui.heading(if total == 0 {
                    "Your library is empty"
                } else if query.is_empty() && self.library_format != LibraryFormat::All {
                    "No games in this format"
                } else {
                    "No matching games"
                });
                ui.label(if total == 0 {
                    "Import one or more .gb or .gbc files to build your collection."
                } else {
                    "Try another search, change the format filter, or clear the search."
                });
                if total == 0 && ui.button("Browse for ROMs…").clicked() {
                    self.choose_content_file();
                }
            });
        } else {
            egui::ScrollArea::vertical()
                .auto_shrink([false, false])
                .show(ui, |ui| {
                    for (title, path, size) in entries {
                        let selected = self.selected_content.as_ref() == Some(&path);
                        egui::Frame::group(ui.style()).show(ui, |ui| {
                            ui.horizontal(|ui| {
                                ui.vertical(|ui| {
                                    ui.label(if selected {
                                        "▶ SELECTED"
                                    } else {
                                        "GAME BOY ROM"
                                    });
                                    ui.heading(&title);
                                    let extension = path
                                        .extension()
                                        .and_then(|value| value.to_str())
                                        .unwrap_or("unknown")
                                        .to_ascii_uppercase();
                                    ui.small(format!(
                                        "{extension} · {} · {}",
                                        format_size(size),
                                        path.display()
                                    ));
                                });
                                ui.with_layout(
                                    egui::Layout::right_to_left(egui::Align::Center),
                                    |ui| {
                                        if ui.button("Forget").clicked() {
                                            match self.controller.forget_content(&path) {
                                                Ok(_) => {
                                                    if self.selected_content.as_ref() == Some(&path)
                                                    {
                                                        self.selected_content = None;
                                                    }
                                                    self.message = format!(
                                                        "Removed '{}' from the library.",
                                                        title
                                                    );
                                                    self.save_preferences();
                                                }
                                                Err(error) => {
                                                    self.message =
                                                        format!("Could not forget content: {error}")
                                                }
                                            }
                                        }
                                        if ui.button("▶ Launch").clicked() {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                            self.launch_content();
                                        }
                                        if ui.button("Use path").clicked() {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                            self.message =
                                                format!("Selected '{}' for launch.", title);
                                        }
                                        if ui
                                            .button(if selected { "Selected" } else { "Select" })
                                            .clicked()
                                        {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                        }
                                    },
                                );
                            });
                        });
                    }
                });
        }

        let selected_details = self.selected_content.clone().and_then(|path| {
            self.controller
                .library()
                .entries()
                .find(|entry| entry.path() == path)
                .map(|entry| (path, entry.title().to_owned(), entry.size_bytes()))
        });
        if let Some((path, title, size)) = selected_details {
            ui.add_space(8.0);
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.vertical(|ui| {
                        ui.heading("Selected content");
                        ui.label(&title);
                        ui.small(path.display().to_string());
                        ui.small(format!(
                            "{} · ready for the shared launch pipeline",
                            format_size(size)
                        ));
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui.button("Launch selected").clicked() {
                            self.content_path = path.display().to_string();
                            self.launch_content();
                        }
                    });
                });
            });
        }
    }

    fn show_sessions(&mut self, ui: &mut egui::Ui) {
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
        let sessions = self.controller.host().session_ids();
        if sessions.is_empty() {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.heading("No open sessions");
                ui.label("Launch a game from your library to create a session.");
                if ui.button("Browse library").clicked() {
                    self.active_page = DesktopPage::Library;
                }
            });
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
                    ui.colored_label(
                        egui::Color32::LIGHT_RED,
                        format!("Last core error: {error}"),
                    );
                }
            });
        }
    }

    fn show_systems(&mut self, ui: &mut egui::Ui) {
        ui.heading("Registered systems");
        ui.label("Capabilities are reported from each core descriptor, not inferred from the UI.");
        ui.add_space(8.0);
        for id in self.registry.system_ids() {
            if let Some(descriptor) = self.registry.registered_descriptor(id) {
                let caps = descriptor.capabilities;
                egui::Frame::group(ui.style()).show(ui, |ui| {
                    ui.heading(descriptor.display_name);
                    ui.small(id.as_str());
                    ui.separator();
                    ui.columns(3, |columns| {
                        columns[0].label(format!(
                            "Content loading: {}",
                            capability_label(caps.content_loading)
                        ));
                        columns[0].label(format!("Input: {}", capability_label(caps.input)));
                        columns[1].label(format!(
                            "Video output: {}",
                            capability_label(caps.video_output)
                        ));
                        columns[1].label(format!(
                            "Audio output: {}",
                            capability_label(caps.audio_output)
                        ));
                        columns[2].label(format!(
                            "Save states: {}",
                            capability_label(caps.save_states)
                        ));
                        columns[2].label(format!(
                            "Persistent saves: {}",
                            capability_label(caps.persistent_saves)
                        ));
                    });
                });
            }
        }
    }

    fn show_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Appearance");
        ui.label(
            "These options change the host interface only; they never change emulated hardware.",
        );
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.checkbox(&mut self.dark_mode, "Use dark theme");
            ui.add(
                egui::Slider::new(&mut self.ui_scale, 0.8..=1.4)
                    .text("Interface scale")
                    .suffix("×"),
            );
        });
        ui.add_space(12.0);
        ui.heading("Application");
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.label("ArchiaEmu is under active development.");
            ui.label("The Game Boy Color core is experimental; launching a ROM does not imply compatibility.");
            ui.label("Imported library paths and appearance preferences are saved in your user configuration.");
            if ui.button("Save settings now").clicked() {
                self.save_preferences();
                self.message = "Settings saved.".to_owned();
            }
        });
    }
}

fn map_key_to_control(key: egui::Key) -> Option<InputControl> {
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

fn video_frame_to_image(frame: &VideoFrame) -> Result<egui::ColorImage, String> {
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
        if let Some(value) = line.strip_prefix("version=") {
            if value != "1" {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    format!("unsupported preferences version: {value}"),
                ));
            }
        } else if let Some(value) = line.strip_prefix("dark=") {
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
    let backup = path.with_extension("conf.bak");
    fs::write(&temporary, contents)?;

    // Windows cannot rename over an existing destination. Move the current file
    // aside first, but restore it if promoting the new file fails.
    let had_previous = path.exists();
    if had_previous {
        if backup.exists() {
            fs::remove_file(&backup)?;
        }
        if let Err(error) = fs::rename(path, &backup) {
            let _ = fs::remove_file(&temporary);
            return Err(error);
        }
    }

    if let Err(error) = fs::rename(&temporary, path) {
        if had_previous {
            let _ = fs::rename(&backup, path);
        }
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }

    if had_previous {
        // The new config is already in place; stale-backup cleanup must not
        // turn a successful save into a reported failure.
        let _ = fs::remove_file(backup);
    }
    Ok(())
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
    fn keyboard_mapping_covers_directions_actions_and_menu_controls() {
        assert_eq!(
            map_key_to_control(egui::Key::ArrowUp),
            Some(InputControl::Up)
        );
        assert_eq!(
            map_key_to_control(egui::Key::Z),
            Some(InputControl::Action1)
        );
        assert_eq!(
            map_key_to_control(egui::Key::Enter),
            Some(InputControl::Start)
        );
        assert_eq!(
            map_key_to_control(egui::Key::Backspace),
            Some(InputControl::Select)
        );
        assert_eq!(map_key_to_control(egui::Key::Escape), None);
    }

    #[test]
    fn video_conversion_removes_stride_padding_without_blurring_rgba_pixels() {
        let frame = VideoFrame::with_stride(
            1,
            2,
            PixelFormat::Rgba8888,
            8,
            vec![255, 0, 0, 255, 99, 99, 99, 99, 0, 255, 0, 255, 88, 88, 88, 88],
        ).unwrap();
        let image = video_frame_to_image(&frame).unwrap();
        assert_eq!(image.size, [1, 2]);
        assert_eq!(image.pixels.len(), 2);
        assert_eq!(image.pixels[0], egui::Color32::from_rgba_unmultiplied(255, 0, 0, 255));
        assert_eq!(image.pixels[1], egui::Color32::from_rgba_unmultiplied(0, 255, 0, 255));
    }

    #[test]
    fn video_conversion_rejects_frames_over_upload_limit_before_copying() {
        let width = 4096u32;
        let height = 4097u32;
        let frame = VideoFrame::new(
            width,
            height,
            PixelFormat::Rgba8888,
            vec![0; (width as usize) * (height as usize) * 4],
        ).unwrap();
        assert!(video_frame_to_image(&frame).unwrap_err().contains("64 MiB"));
    }

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
