//! settings page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_settings(&mut self, ui: &mut egui::Ui, context: &egui::Context) {
        ui.heading(if self.language == "fr" { "Paramètres · apparence" } else { "Host settings · appearance" });
        ui.label(if self.language == "fr" {
            "Ces options modifient uniquement l’interface, pas le matériel émulé."
        } else {
            "These options change the host interface only; they never change emulated hardware."
        });
        ui.horizontal(|ui| {
            ui.label(if self.language == "fr" { "Langue de l’application" } else { "Application language" });
            let previous_language = self.language.clone();
            egui::ComboBox::from_id_source("application-language")
                .selected_text(if self.language == "fr" { "Français" } else { "English" })
                .show_ui(ui, |ui| {
                    ui.selectable_value(&mut self.language, "fr".to_owned(), "Français");
                    ui.selectable_value(&mut self.language, "en".to_owned(), "English");
                });
            if previous_language != self.language {
                self.message = if self.language == "fr" { "Langue modifiée." } else { "Language changed." }.to_owned();
                self.save_preferences();
            }
        });
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.checkbox(&mut self.dark_mode, if self.language == "fr" { "Utiliser le thème sombre" } else { "Use dark theme" });
            ui.horizontal_wrapped(|ui| {
                ui.label(if self.language == "fr" { "Couleur d’accentuation" } else { "Accent color" });
                ui.color_edit_button_srgba(&mut self.accent_color);
                ui.monospace(format!(
                    "#{:02X}{:02X}{:02X}",
                    self.accent_color.r(),
                    self.accent_color.g(),
                    self.accent_color.b()
                ));
                if ui.button(if self.language == "fr" { "Réinitialiser" } else { "Reset" }).clicked() {
                    self.accent_color = egui::Color32::from_rgb(75, 115, 210);
                }
            });
            ui.label(
                if self.language == "fr" { "Modifie les boutons, la sélection et les surbrillances, pas l’arrière-plan." } else { "Changes the buttons, selection and highlights—not the light or dark backgrounds." },
            );
            ui.add(
                egui::Slider::new(&mut self.ui_scale, 0.8..=1.4)
                    .text(if self.language == "fr" { "Échelle de l’interface" } else { "Interface scale" })
                    .suffix("×"),
            );
        });
        ui.add_space(12.0);
        ui.heading(if self.language == "fr" { "Paramètres · application" } else { "Host settings · application" });
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.label("ArchiaEmu is under active development.");
            ui.label("The Game Boy Color core is experimental; launching a ROM does not imply compatibility.");
            ui.label("Imported library paths and appearance preferences are saved in your user configuration.");
            if ui.button(if self.language == "fr" { "Enregistrer les paramètres" } else { "Save settings now" }).clicked() && self.save_preferences() {
                self.message = "Settings saved.".to_owned();
            }
        });

        ui.add_space(12.0);
        ui.heading(if self.language == "fr" { "Paramètres du cœur · indisponibles" } else { "Core settings · not available yet" });
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.label("These settings belong to a future emulation core and are deliberately separate from host appearance preferences.");
            ui.label("No core-specific hardware settings are currently exposed by this application.");
            ui.add_enabled(false, egui::Button::new("Core settings will appear when a core supports them"));
        });

        ui.add_space(12.0);
        ui.heading(if self.language == "fr" { "Mises à jour de l’application" } else { "Application updates" });
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.label("Checks the official stable application manifest. Module downloads are independent.");
            ui.horizontal_wrapped(|ui| {
                let checking = self.update_check_receiver.is_some();
                if ui
                    .add_enabled(!checking, egui::Button::new(if self.language == "fr" { "Rechercher des mises à jour" } else { "Check for updates" }))
                    .clicked()
                {
                    let (sender, receiver) = mpsc::channel();
                    self.update_check_receiver = Some(receiver);
                    self.update_status = "Checking the official stable manifest…".to_owned();
                    std::thread::spawn(move || {
                        let _ = sender.send(update::fetch_manifest());
                    });
                    context.request_repaint_after(Duration::from_millis(250));
                }
                ui.label(&self.update_status);
            });
            let update_info = self.update_manifest.as_ref().map(|manifest| {
                (
                    manifest.version.clone(),
                    manifest.release_notes_url.clone(),
                    manifest.download_for_current_platform().cloned(),
                )
            });
            if let Some((version, release_notes_url, download)) = update_info {
                ui.horizontal_wrapped(|ui| {
                    ui.label(format!("Manifest version: {version}"));
                    ui.hyperlink_to("Release notes", &release_notes_url);
                });
                let current = option_env!("CARGO_PKG_VERSION").unwrap_or("0.0.0");
                let update_available = update::is_newer_version(&version, current);
                if update_available {
                    ui.label(format!("Update available: {current} → {version}"));
                    if let Some(download) = download {
                        let downloading = self.update_download_receiver.is_some();
                        if ui
                            .add_enabled(
                                !downloading,
                                egui::Button::new("Download and verify update"),
                            )
                            .clicked()
                        {
                            let (sender, receiver) = mpsc::channel();
                            self.update_download_receiver = Some(receiver);
                            self.update_status = format!(
                                "Downloading version {version} to a temporary staging area…"
                            );
                            std::thread::spawn(move || {
                                let result = update::download_and_verify(&download, &version);
                                let _ = sender.send(result);
                            });
                            context.request_repaint_after(Duration::from_millis(250));
                        }
                    } else {
                        ui.small("No installable update artifact is currently published for this platform.");
                    }
                } else if update::is_newer_version(current, &version) {
                    ui.label(format!(
                        "Installed version ({current}) is newer than the stable manifest ({version})."
                    ));
                } else {
                    ui.label(format!("ArchiaEmu is up to date ({current})."));
                }
            }
            if let Some(path) = self.staged_update_path.as_ref() {
                ui.label(format!("Verified staged file: {}", path.display()));
                ui.small("The file is verified but has not been installed or executed.");
            }
            ui.small("Automatic installation and rollback are not enabled yet.");
        });

        ui.add_space(12.0);
        ui.heading(if self.language == "fr" { "Diagnostic" } else { "Diagnostics" });
        ui.label("Local runtime information to help troubleshoot startup and session issues.");
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.label("Application version:");
                ui.monospace(option_env!("CARGO_PKG_VERSION").unwrap_or("unknown"));
                ui.separator();
                ui.label("Platform:");
                ui.monospace(format!("{} / {}", std::env::consts::OS, std::env::consts::ARCH));
            });
            ui.horizontal_wrapped(|ui| {
                ui.label(format!("Library items: {}", self.controller.library().len()));
                ui.separator();
                ui.label(format!("Open sessions: {}", self.controller.session_summaries().len()));
                ui.separator();
                ui.label(format!("Execution errors: {}", self.last_tick_errors.len()));
            });
            ui.horizontal_wrapped(|ui| {
                ui.label("Active session:");
                ui.monospace(
                    self.controller
                        .active_session()
                        .map(|id| id.get().to_string())
                        .unwrap_or_else(|| "none".to_owned()),
                );
            });
            ui.label("Configuration file:");
            ui.monospace(
                self.config_path
                    .as_deref()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|| "unavailable".to_owned()),
            );
            ui.label("Media status:");
            ui.label(&self.media_status);
            ui.label("Update status:");
            ui.label(&self.update_status);
            ui.label("Staged update:");
            ui.label(
                self.staged_update_path
                    .as_deref()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|| "none".to_owned()),
            );
            ui.label("Latest application message:");
            ui.label(&self.message);
            if !self.last_tick_errors.is_empty() {
                ui.colored_label(egui::Color32::LIGHT_RED, "Recent session errors");
                for (session_id, error) in self.last_tick_errors.iter().take(5) {
                    ui.label(format!("Session {}: {error}", session_id.get()));
                }
            }
            if ui.button(if self.language == "fr" { "Copier le diagnostic" } else { "Copy diagnostics to clipboard" }).clicked() {
                let recent_errors = self
                    .last_tick_errors
                    .iter()
                    .take(5)
                    .map(|(session_id, error)| format!("Session {}: {error}", session_id.get()))
                    .collect::<Vec<_>>()
                    .join("\n");
                let active_session = self
                    .controller
                    .active_session()
                    .map(|id| id.get().to_string())
                    .unwrap_or_else(|| "none".to_owned());
                let report = format!(
                    "ArchiaEmu {}\nPlatform: {} / {}\nLibrary items: {}\nOpen sessions: {}\nActive session: {}\nExecution errors: {}\nConfiguration: {}\nMedia status: {}\nUpdate status: {}\nStaged update: {}\nLatest message: {}\nRecent errors:\n{}",
                    option_env!("CARGO_PKG_VERSION").unwrap_or("unknown"),
                    std::env::consts::OS,
                    std::env::consts::ARCH,
                    self.controller.library().len(),
                    self.controller.session_summaries().len(),
                    active_session,
                    self.last_tick_errors.len(),
                    self.config_path
                        .as_deref()
                        .map(|path| path.display().to_string())
                        .unwrap_or_else(|| "unavailable".to_owned()),
                    self.media_status,
                    self.update_status,
                    self.staged_update_path
                        .as_deref()
                        .map(|path| path.display().to_string())
                        .unwrap_or_else(|| "none".to_owned()),
                    self.message,
                    recent_errors
                );
                ui.output_mut(|output| output.copied_text = report);
                self.message = "Diagnostics copied to clipboard.".to_owned();
            }
        });
    }
}
