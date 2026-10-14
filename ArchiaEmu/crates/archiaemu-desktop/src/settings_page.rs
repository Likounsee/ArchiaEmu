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
    }
}
