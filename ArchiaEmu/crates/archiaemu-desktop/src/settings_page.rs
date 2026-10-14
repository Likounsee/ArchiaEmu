//! settings page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_settings(&mut self, ui: &mut egui::Ui) {
        ui.heading("Appearance");
        ui.label(
            "These options change the host interface only; they never change emulated hardware.",
        );
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.checkbox(&mut self.dark_mode, "Use dark theme");
            ui.horizontal_wrapped(|ui| {
                ui.label("Accent color");
                ui.color_edit_button_srgba(&mut self.accent_color);
                ui.monospace(format!(
                    "#{:02X}{:02X}{:02X}",
                    self.accent_color.r(),
                    self.accent_color.g(),
                    self.accent_color.b()
                ));
                if ui.button("Reset").clicked() {
                    self.accent_color = egui::Color32::from_rgb(75, 115, 210);
                }
            });
            ui.label(
                "Changes the buttons, selection and highlights—not the light or dark backgrounds.",
            );
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

        ui.add_space(12.0);
        ui.heading("Diagnostics");
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
            ui.label("Latest application message:");
            ui.label(&self.message);
            if !self.last_tick_errors.is_empty() {
                ui.colored_label(egui::Color32::LIGHT_RED, "Recent session errors");
                for (session_id, error) in self.last_tick_errors.iter().take(5) {
                    ui.label(format!("Session {}: {error}", session_id.get()));
                }
            }
            if ui.button("Copy diagnostics to clipboard").clicked() {
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
                    "ArchiaEmu {}\nPlatform: {} / {}\nLibrary items: {}\nOpen sessions: {}\nActive session: {}\nExecution errors: {}\nConfiguration: {}\nMedia status: {}\nLatest message: {}\nRecent errors:\n{}",
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
                    self.message,
                    recent_errors
                );
                ui.output_mut(|output| output.copied_text = report);
                self.message = "Diagnostics copied to clipboard.".to_owned();
            }
        });
    }
}
