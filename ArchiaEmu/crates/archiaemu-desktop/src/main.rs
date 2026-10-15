#![forbid(unsafe_code)]
#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]
//! Experimental desktop frontend for the shared ArchiaEmu application controller.

use std::error::Error;
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, Receiver, TryRecvError};
use std::time::{Duration, Instant};

use archiaemu_app::controller::{ApplicationController, SessionCommand};
use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::core_io::{FrameCadence, InputControl, InputEvent};
use audio_output::AudioOutput;
use game_view::{map_key_to_control, video_frame_to_image};
use preferences::{config_file_path, load_preferences, write_preferences_with_language};

mod audio_output;
mod game_page;
mod game_view;
mod home_page;
mod library_page;
mod preferences;
mod sessions_page;

mod settings_page;
mod systems_page;
mod update;
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeConfig, SessionId};
use archiaemu_core::runtime::SessionState;
use archiaemu_core::CoreRegistry;
use archiaemu_game_boy::{register_game_boy, GameBoyCartridgeProbe};
use eframe::egui;

const MAX_CONTENT_BYTES: usize = 32 * 1024 * 1024;

fn main() -> Result<(), Box<dyn Error>> {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    if let Some(message) = parse_cli_arguments(&arguments)? {
        println!("{message}");
        return Ok(());
    }

    let app = DesktopApp::new()?;
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1280.0, 820.0])
            .with_min_inner_size([640.0, 480.0]),
        ..Default::default()
    };
    eframe::run_native(
        "ArchiaEmu",
        options,
        Box::new(move |_creation_context| Box::new(app)),
    )?;
    Ok(())
}

fn parse_cli_arguments(arguments: &[String]) -> Result<Option<&'static str>, String> {
    if arguments.len() > 1 {
        return Err("expected at most one command-line argument; try --help".to_owned());
    }
    arguments
        .first()
        .map(|argument| cli_message(argument))
        .transpose()
}

fn cli_message(argument: &str) -> Result<&'static str, String> {
    match argument {
        "--version" | "-V" => Ok(concat!("ArchiaEmu ", env!("CARGO_PKG_VERSION"))),
        "--help" | "-h" => Ok(concat!(
            "ArchiaEmu ",
            env!("CARGO_PKG_VERSION"),
            "\n\nUsage: archiaemu-desktop [OPTIONS]\n\nOptions:\n  -h, --help     Print this help message\n  -V, --version  Print version information\n"
        )),
        other => Err(format!("unknown argument: {other}; try --help")),
    }
}

fn clear_tick_error_for_session(errors: &mut Vec<(SessionId, String)>, id: SessionId) {
    errors.retain(|(error_id, _)| *error_id != id);
}

fn message_indicates_error(message: &str) -> bool {
    let message = message.to_ascii_lowercase();
    [
        "import failed:",
        "launch failed:",
        "launch cancelled:",
        "could not ",
        "preferences unavailable:",
        "input error:",
    ]
    .iter()
    .any(|prefix| message.starts_with(prefix))
        || (message.starts_with("session ") && message.contains(" failed:"))
}

/// Polls a background task and drops its receiver on success or disconnection.
/// A disconnected worker must not leave the UI repainting forever as if it were still running.
fn poll_background_result<T>(receiver: &mut Option<Receiver<T>>) -> Result<Option<T>, ()> {
    match receiver.as_ref().map(Receiver::try_recv) {
        None | Some(Err(TryRecvError::Empty)) => Ok(None),
        Some(Ok(result)) => {
            *receiver = None;
            Ok(Some(result))
        }
        Some(Err(TryRecvError::Disconnected)) => {
            *receiver = None;
            Err(())
        }
    }
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
    language: String,
    ui_scale: f32,
    accent_color: egui::Color32,
    config_path: Option<PathBuf>,
    active_page: DesktopPage,
    last_tick: Instant,
    frame_cadence: FrameCadence,
    frame_texture: Option<egui::TextureHandle>,
    video_size: Option<[u32; 2]>,
    media_status: String,
    audio_output: AudioOutput,
    last_tick_errors: Vec<(SessionId, String)>,
    session_content_paths: Vec<(SessionId, PathBuf)>,
    update_check_receiver: Option<Receiver<Result<update::UpdateManifest, String>>>,
    update_download_receiver: Option<Receiver<Result<PathBuf, String>>>,
    update_manifest: Option<update::UpdateManifest>,
    staged_update_path: Option<PathBuf>,
    update_status: String,
    fullscreen: bool,
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

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum UiStatus {
    Loading,
    Success,
    Error,
    ContentAbsent,
    CapabilityUnavailable,
    Information,
}

impl UiStatus {
    fn label(self) -> &'static str {
        match self {
            Self::Loading => "Loading",
            Self::Success => "Success",
            Self::Error => "Error",
            Self::ContentAbsent => "No content",
            Self::CapabilityUnavailable => "Capability unavailable",
            Self::Information => "Ready",
        }
    }

    fn color(self) -> egui::Color32 {
        match self {
            Self::Loading => egui::Color32::LIGHT_BLUE,
            Self::Success => egui::Color32::from_rgb(95, 190, 140),
            Self::Error => egui::Color32::LIGHT_RED,
            Self::ContentAbsent => egui::Color32::GRAY,
            Self::CapabilityUnavailable => egui::Color32::from_rgb(220, 160, 70),
            Self::Information => egui::Color32::from_rgb(130, 160, 190),
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
    fn title(self, french: bool) -> &'static str {
        match self {
            Self::Home => if french { "Accueil" } else { "Overview" },
            Self::Library => if french { "Bibliothèque" } else { "Game library" },
            Self::Sessions => "Sessions",
            Self::Game => if french { "Jeu" } else { "Game" },
            Self::Systems => if french { "Systèmes" } else { "Systems" },
            Self::Settings => if french { "Paramètres" } else { "Settings" },
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
        let mut language = preferences::system_language();
        let mut ui_scale = 1.0;
        let mut accent_color = egui::Color32::from_rgb(75, 115, 210);
        let mut message = String::from("Ready. Select a content path to import or launch.");
        if let Some(path) = config_path.as_deref() {
            match load_preferences(path) {
                Ok(Some(saved)) => {
                    dark_mode = saved.dark_mode;
                    language = saved.language;
                    ui_scale = saved.ui_scale.clamp(0.8, 1.4);
                    accent_color = egui::Color32::from_rgb(
                        saved.accent_rgb[0],
                        saved.accent_rgb[1],
                        saved.accent_rgb[2],
                    );
                    let mut restored = 0usize;
                    let mut skipped = 0usize;
                    let mut skipped_details = Vec::new();
                    for content_path in saved.paths {
                        match controller.import_content(&content_path, MAX_CONTENT_BYTES as u64) {
                            Ok(_) => restored += 1,
                            Err(error) => {
                                skipped += 1;
                                if skipped_details.len() < 3 {
                                    skipped_details
                                        .push(format!("{}: {error}", content_path.display()));
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
            language,
            ui_scale,
            accent_color,
            config_path,
            active_page: DesktopPage::Home,
            last_tick: Instant::now(),
            frame_cadence: FrameCadence::new(60, 3)?,
            frame_texture: None,
            video_size: None,
            media_status: "No active session.".to_owned(),
            audio_output: AudioOutput::new(),
            last_tick_errors: Vec::new(),
            session_content_paths: Vec::new(),
            update_check_receiver: None,
            update_download_receiver: None,
            update_manifest: None,
            staged_update_path: None,
            update_status: "Not checked yet.".to_owned(),
            fullscreen: false,
        })
    }

    fn navigate_to(&mut self, page: DesktopPage) {
        if self.active_page == DesktopPage::Game && page != DesktopPage::Game {
            if let Err(error) = self.release_game_inputs() {
                self.message = format!(
                    "{error}. Navigation was cancelled so the game can safely release its controls."
                );
                return;
            }
        }
        self.active_page = page;
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
            let import_message = if rejected == 0 {
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
            self.message = import_message.clone();
            if imported > 0 {
                self.navigate_to(DesktopPage::Library);
                if !self.save_preferences() {
                    let save_error = std::mem::take(&mut self.message);
                    self.message = format!(
                        "{import_message} Library changes are active for this session, but preferences were not saved. {save_error}"
                    );
                }
            }
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
                let import_message = format!(
                    "Imported '{}' ({}).",
                    entry.title(),
                    format_size(entry.size_bytes())
                );
                self.message = import_message.clone();
                self.navigate_to(DesktopPage::Library);
                if !self.save_preferences() {
                    let save_error = std::mem::take(&mut self.message);
                    self.message = format!(
                        "{import_message} The import is active for this session, but preferences were not saved. {save_error}"
                    );
                }
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
        // Release held controls before creating/switching to another session. If this
        // fails, abort the launch: reporting the failure after switching sessions could
        // leave the previous session with logically held input and no safe recovery path.
        if let Some(previous_id) = self.controller.active_session() {
            if let Err(error) = self.controller.release_inputs(previous_id) {
                self.message = format!(
                    "Launch cancelled: could not release inputs from session {}: {error}",
                    previous_id.get()
                );
                return;
            }
        }

        let probes: [&dyn ContentFormatProbe; 1] = [&self.probe];
        match self.controller.launch_content_path(
            Path::new(path),
            ContentLimits::new(MAX_CONTENT_BYTES),
            &probes,
            &self.registry,
        ) {
            Ok(id) => {
                self.audio_output.clear();
                let launched_path =
                    std::fs::canonicalize(Path::new(path)).unwrap_or_else(|_| PathBuf::from(path));
                self.session_content_paths
                    .retain(|(session_id, _)| *session_id != id);
                self.session_content_paths.push((id, launched_path));
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

    fn ui_status(&self) -> UiStatus {
        if self.update_check_receiver.is_some() || self.update_download_receiver.is_some() {
            return UiStatus::Loading;
        }

        if message_indicates_error(&self.message) {
            return UiStatus::Error;
        }

        if (self.active_page == DesktopPage::Library && self.controller.library().is_empty())
            || (self.active_page == DesktopPage::Sessions && self.controller.session_count() == 0)
            || (self.active_page == DesktopPage::Game && self.controller.active_session().is_none())
        {
            return UiStatus::ContentAbsent;
        }

        if self.active_page == DesktopPage::Game
            && (self.media_status.contains("does not currently advertise")
                || self.media_status.contains("unavailable"))
        {
            return UiStatus::CapabilityUnavailable;
        }

        if self.message.starts_with("imported ")
            || self.message.starts_with("launched ")
            || self.message.starts_with("closed ")
            || self.message.starts_with("removed ")
            || self.message.contains(" succeeded.")
            || self.message.starts_with("settings saved")
        {
            return UiStatus::Success;
        }

        UiStatus::Information
    }

    fn save_preferences(&mut self) -> bool {
        let Some(path) = self.config_path.as_deref() else {
            self.message =
                "Could not save preferences: no writable user configuration directory is available."
                    .to_owned();
            return false;
        };
        let paths: Vec<PathBuf> = self
            .controller
            .library()
            .entries()
            .map(|entry| entry.path().to_path_buf())
            .collect();
        if let Err(error) = write_preferences_with_language(
            path,
            self.dark_mode,
            self.ui_scale,
            [
                self.accent_color.r(),
                self.accent_color.g(),
                self.accent_color.b(),
            ],
            &self.language,
            &paths,
        ) {
            self.message = format!("Could not save preferences: {error}");
            return false;
        }
        true
    }

    fn session_action(&mut self, id: SessionId, action: SessionAction) {
        let is_active = self.controller.active_session() == Some(id);
        let release_error =
            if is_active && matches!(action, SessionAction::Stop | SessionAction::Reset) {
                self.release_game_inputs().err()
            } else {
                None
            };

        // A reset is only safe once held controls have been released. Stop, however, must
        // remain available as a recovery action even if releasing input itself fails.
        if matches!(action, SessionAction::Reset) {
            if let Some(error) = release_error.as_ref() {
                self.message = format!(
                    "Session {}: reset aborted because input release failed: {error}",
                    id.get()
                );
                return;
            }
        }

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
        if result.is_ok() && is_active {
            match action {
                SessionAction::Pause => {
                    self.audio_output.pause();
                    self.frame_cadence.reset();
                }
                SessionAction::Resume => {
                    self.audio_output.resume();
                    self.last_tick = Instant::now();
                }
                SessionAction::Stop => {
                    self.audio_output.clear();
                    self.frame_cadence.reset();
                    self.frame_texture = None;
                    self.video_size = None;
                    self.media_status = "Active session stopped.".to_owned();
                }
                SessionAction::Reset => {
                    self.audio_output.clear();
                    self.frame_cadence.reset();
                    self.frame_texture = None;
                    self.video_size = None;
                    self.last_tick = Instant::now();
                    self.media_status = "Active session reset; waiting for a new frame.".to_owned();
                }
            }
        }
        if result.is_ok() && matches!(action, SessionAction::Stop | SessionAction::Reset) {
            // A deliberate lifecycle transition supersedes an earlier execution/media tick error.
            clear_tick_error_for_session(&mut self.last_tick_errors, id);
        }
        self.message = match (result, release_error) {
            (Ok(()), Some(error)) => format!(
                "Session {}: {} succeeded, but input release failed: {error}",
                id.get(),
                action.label()
            ),
            (Ok(()), None) => format!("Session {}: {} succeeded.", id.get(), action.label()),
            (Err(error), _) => {
                format!("Session {}: {} failed: {error}", id.get(), action.label())
            }
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

fn navigation_items(french: bool) -> [(DesktopPage, &'static str, &'static str); 6] {
    // Use common glyphs supported by the bundled default font; avoid the
    // uncommon filled-square symbols that appeared as missing-glyph boxes.
    [
        (DesktopPage::Home, "⌂", if french { "Accueil" } else { "Overview" }),
        (DesktopPage::Library, "≡", if french { "Bibliothèque" } else { "Game library" }),
        (DesktopPage::Sessions, "○", "Sessions"),
        (DesktopPage::Game, "▶", if french { "Jeu" } else { "Game" }),
        (DesktopPage::Systems, "◎", if french { "Systèmes" } else { "Systems" }),
        (DesktopPage::Settings, "*", if french { "Paramètres" } else { "Settings" }),
    ]
}

impl eframe::App for DesktopApp {
    fn update(&mut self, context: &egui::Context, _frame: &mut eframe::Frame) {
        let update_result = match poll_background_result(&mut self.update_check_receiver) {
            Ok(result) => result,
            Err(()) => {
                self.update_status = "Update check failed: background worker disconnected before returning a result.".to_owned();
                self.update_manifest = None;
                None
            }
        };
        if let Some(result) = update_result {
            match result {
                Ok(manifest) => {
                    let current = option_env!("CARGO_PKG_VERSION").unwrap_or("0.0.0");
                    self.update_status = if update::is_newer_version(&manifest.version, current) {
                        if manifest.download_for_current_platform().is_some() {
                            format!("Version {} is available for download and integrity verification; installation is not automatic yet.", manifest.version)
                        } else {
                            format!("Version {} is published, but no download is available for this platform yet.", manifest.version)
                        }
                    } else {
                        format!("You are using the latest stable version ({}).", current)
                    };
                    self.update_manifest = Some(manifest);
                }
                Err(error) => {
                    self.update_status = format!("Update check failed: {error}");
                    self.update_manifest = None;
                }
            }
        }
        let download_result = match poll_background_result(&mut self.update_download_receiver) {
            Ok(result) => result,
            Err(()) => {
                self.update_status = "Update download failed: background worker disconnected before returning a result.".to_owned();
                self.staged_update_path = None;
                None
            }
        };
        if let Some(result) = download_result {
            match result {
                Ok(path) => {
                    self.update_status = format!(
                        "Update downloaded and SHA-256 verified at {}. Installation is not automatic.",
                        path.display()
                    );
                    self.staged_update_path = Some(path);
                }
                Err(error) => {
                    self.update_status = format!("Update download failed: {error}");
                    self.staged_update_path = None;
                }
            }
        }
        if self.update_check_receiver.is_some() || self.update_download_receiver.is_some() {
            context.request_repaint_after(Duration::from_millis(250));
        }
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
            style.visuals.widgets.inactive.bg_stroke =
                egui::Stroke::new(1.0_f32, accent.gamma_multiply(0.45));
            style.visuals.widgets.hovered.bg_fill = accent.gamma_multiply(0.22);
            style.visuals.widgets.hovered.bg_stroke = egui::Stroke::new(1.0_f32, accent);
            style.visuals.widgets.active.bg_fill = accent.gamma_multiply(0.38);
            style.visuals.widgets.active.bg_stroke = egui::Stroke::new(1.0_f32, accent);
            style.visuals.widgets.inactive.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.hovered.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.active.rounding = egui::Rounding::same(6.0);
            style.visuals.widgets.noninteractive.rounding = egui::Rounding::same(6.0);
        });

        let compact_navigation = context.input(|input| input.screen_rect().width() < 980.0);

        if !compact_navigation {
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
                    for (page, glyph, label) in navigation_items(self.language == "fr") {
                        if ui
                            .selectable_label(
                                self.active_page == page,
                                format!("{glyph}   {label}"),
                            )
                            .clicked()
                        {
                            self.navigate_to(page);
                        }
                    }
                    ui.with_layout(egui::Layout::bottom_up(egui::Align::Min), |ui| {
                        ui.separator();
                        ui.label("EARLY DEVELOPMENT");
                        ui.small("Core support is experimental.");
                        ui.small("No game compatibility is implied.");
                    });
                });
        } else {
            egui::TopBottomPanel::top("compact_navigation").show(context, |ui| {
                egui::ScrollArea::horizontal().show(ui, |ui| {
                    ui.horizontal(|ui| {
                        ui.strong("◈ ArchiaEmu");
                        ui.separator();
                        for (page, glyph, label) in navigation_items() {
                            if ui
                                .selectable_label(
                                    self.active_page == page,
                                    format!("{glyph} {label}"),
                                )
                                .clicked()
                            {
                                self.navigate_to(page);
                            }
                        }
                    });
                });
            });
        }

        egui::TopBottomPanel::top("app_header").show(context, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.vertical(|ui| {
                    ui.heading(self.active_page.title(self.language == "fr"));
                    ui.label(if self.language == "fr" { "Votre bibliothèque, vos sessions et vos outils d’émulation au même endroit." } else { "Your library, sessions and emulator tools in one place." });
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui
                        .button(if self.fullscreen {
                            "⛶ Windowed"
                        } else {
                            "⛶ Fullscreen"
                        })
                        .clicked()
                    {
                        self.fullscreen = !self.fullscreen;
                        context
                            .send_viewport_cmd(egui::ViewportCommand::Fullscreen(self.fullscreen));
                    }
                    ui.label(if self.dark_mode {
                        if self.language == "fr" { "◐ Sombre" } else { "◐ Dark" }
                    } else if self.language == "fr" { "☼ Clair" } else { "☼ Light" });
                    ui.label(format!("{} session(s)", self.controller.session_count()));
                });
            });
        });

        egui::CentralPanel::default().show(context, |ui| {
            // Keep long pages usable on short displays and when the window is resized.
            egui::ScrollArea::vertical()
                .auto_shrink([false, false])
                .show(ui, |ui| {
                    match self.active_page {
                        DesktopPage::Home => self.show_home(ui),
                        DesktopPage::Library => self.show_library(ui),
                        DesktopPage::Sessions => self.show_sessions(ui),
                        DesktopPage::Game => self.show_game(ui),
                        DesktopPage::Systems => self.show_systems(ui),
                        DesktopPage::Settings => self.show_settings(ui, context),
                    }
                    ui.add_space(12.0);
                    ui.separator();
                    ui.horizontal_wrapped(|ui| {
                        let status = self.ui_status();
                        ui.colored_label(status.color(), format!("● {}", status.label()));
                        ui.label(&self.message);
                    });
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
                    egui::Event::WindowFocused(false) => {
                        if let Err(error) = self.release_game_inputs() {
                            self.message = error;
                        }
                    }
                    _ => {}
                }
            }
        }

        let Some(id) = self.controller.active_session() else {
            self.frame_cadence.reset();
            self.frame_texture = None;
            self.video_size = None;
            self.media_status =
                "No active session. Launch a ROM to receive core-provided frames.".to_owned();
            return;
        };

        let update = match self
            .controller
            .advance_session(id, elapsed, &mut self.frame_cadence)
        {
            Ok(update) => update,
            Err(error) => {
                let release_status = if self.controller.active_session() == Some(id) {
                    self.controller
                        .release_inputs(id)
                        .err()
                        .map(|release| format!(" Input release also failed: {release}"))
                        .unwrap_or_default()
                } else {
                    String::new()
                };
                self.media_status =
                    format!("Could not advance selected session: {error}.{release_status}");
                self.last_tick_errors
                    .retain(|(error_id, _)| *error_id != id);
                self.last_tick_errors.push((id, self.media_status.clone()));
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
        if state == SessionState::Running {
            for buffer in &update.audio_buffers {
                self.audio_output.submit(buffer);
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

        if let Some(error_message) = error.as_ref() {
            let mut diagnostic = error_message.clone();
            if self.controller.active_session() == Some(id) {
                if let Err(release_error) = self.controller.release_inputs(id) {
                    diagnostic.push_str(&format!(" Input release also failed: {release_error}"));
                }
            }
            self.last_tick_errors
                .retain(|(error_id, _)| *error_id != id);
            self.last_tick_errors.push((id, diagnostic));
            self.frame_texture = None;
            self.video_size = None;
        } else {
            self.last_tick_errors
                .retain(|(error_id, _)| *error_id != id);
        }

        let pipeline_status = if let Some(error) = error {
            error
        } else if state == SessionState::Paused {
            "Paused — video output is frozen until you resume.".to_owned()
        } else if state != SessionState::Running {
            format!("Session state: {state:?}.")
        } else if !video_available {
            "This core does not currently advertise video output; the display area is a placeholder.".to_owned()
        } else if audio_available {
            "Video pipeline connected; core audio buffers are forwarded to the host output."
                .to_owned()
        } else {
            "Video pipeline connected. This core does not currently advertise audio output."
                .to_owned()
        };
        self.media_status = if audio_available {
            format!(
                "{pipeline_status} Audio device: {}.",
                self.audio_output.status()
            )
        } else {
            pipeline_status
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

    fn release_game_inputs(&mut self) -> Result<(), String> {
        let Some(id) = self.controller.active_session() else {
            return Ok(());
        };
        self.controller
            .release_inputs(id)
            .map_err(|error| format!("Could not release held inputs: {error}"))
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
    use crate::preferences::{
        escape_config_value, load_preferences, unescape_config_value, write_preferences,
    };
    use std::fs;

    #[test]
    fn launch_cancellation_is_classified_as_an_error() {
        assert!(message_indicates_error(
            "Launch cancelled: could not release inputs from session 3"
        ));
        assert!(!message_indicates_error("Launched session 3."));
    }

    #[test]
    fn command_line_help_and_version_are_available_without_starting_the_gui() {
        assert_eq!(
            cli_message("--version").unwrap(),
            concat!("ArchiaEmu ", env!("CARGO_PKG_VERSION"))
        );
        assert!(cli_message("--help").unwrap().contains("--version"));
        assert!(cli_message("-h")
            .unwrap()
            .contains("Usage: archiaemu-desktop"));
        assert_eq!(
            cli_message("--unknown").unwrap_err(),
            "unknown argument: --unknown; try --help"
        );
    }

    #[test]
    fn cli_argument_parser_rejects_multiple_arguments_before_gui_startup() {
        let arguments = vec!["--help".to_owned(), "--version".to_owned()];
        assert_eq!(
            parse_cli_arguments(&arguments).unwrap_err(),
            "expected at most one command-line argument; try --help"
        );
    }

    #[test]
    fn cli_argument_parser_leaves_gui_startup_to_the_caller_without_options() {
        assert_eq!(parse_cli_arguments(&[]).unwrap(), None);
        assert!(parse_cli_arguments(&["--version".to_owned()])
            .unwrap()
            .unwrap()
            .starts_with("ArchiaEmu "));
    }

    #[test]
    fn user_status_classifies_each_error_family_without_false_positives() {
        for message in [
            "Import failed: file is unreadable",
            "Launch failed: format was rejected",
            "Launch cancelled: inputs could not be released",
            "Could not load preferences: invalid data",
            "Preferences unavailable: no config directory",
            "Input error: device disconnected",
            "Session 7 failed: core stopped unexpectedly",
        ] {
            assert!(
                message_indicates_error(message),
                "not classified as error: {message}"
            );
        }
        for message in [
            "Ready. Select a content path to import or launch.",
            "Restored preferences and 2 library item(s).",
            "Launched session 7.",
            "No active session.",
        ] {
            assert!(
                !message_indicates_error(message),
                "false error classification: {message}"
            );
        }
    }

    #[test]
    fn file_size_formatting_changes_units_at_exact_boundaries() {
        assert_eq!(format_size(1023), "1023 B");
        assert_eq!(format_size(1024), "1.0 KiB");
        assert_eq!(format_size(1024 * 1024 - 1), "1024.0 KiB");
        assert_eq!(format_size(1024 * 1024), "1.00 MiB");
    }

    #[test]
    fn background_task_receiver_is_cleared_after_worker_disconnects() {
        let (sender, receiver) = mpsc::channel::<u8>();
        drop(sender);
        let mut receiver = Some(receiver);

        assert!(poll_background_result(&mut receiver).is_err());
        assert!(receiver.is_none());
    }

    #[test]
    fn background_task_receiver_returns_results_and_waits_while_empty() {
        let (sender, receiver) = mpsc::channel::<u8>();
        let mut receiver = Some(receiver);

        assert_eq!(poll_background_result(&mut receiver), Ok(None));
        sender.send(42).unwrap();
        assert_eq!(poll_background_result(&mut receiver), Ok(Some(42)));
        assert!(receiver.is_none());
    }

    #[test]
    fn clearing_one_session_tick_error_preserves_other_sessions() {
        let mut controller =
            ApplicationController::new(HostRuntime::new(HostRuntimeConfig::default()).unwrap());
        let first = controller
            .host_mut()
            .create_session(Box::new(archiaemu_game_boy::GameBoyCore::new()))
            .unwrap();
        let second = controller
            .host_mut()
            .create_session(Box::new(archiaemu_game_boy::GameBoyCore::new()))
            .unwrap();
        let mut errors = vec![
            (first, "first session failed".to_owned()),
            (second, "second session failed".to_owned()),
        ];

        clear_tick_error_for_session(&mut errors, first);

        assert_eq!(errors, vec![(second, "second session failed".to_owned())]);
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
        write_preferences(
            &file,
            false,
            2.0,
            [75, 115, 210],
            &[PathBuf::from("sample.gb")],
        )
        .unwrap();
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
        write_preferences(&file, true, 1.0, [75, 115, 210], &[PathBuf::from("old.gb")]).unwrap();
        write_preferences(
            &file,
            false,
            1.2,
            [75, 115, 210],
            &[PathBuf::from("new.gb")],
        )
        .unwrap();

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
