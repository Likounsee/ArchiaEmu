//! systems page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_systems(&mut self, ui: &mut egui::Ui) {
        let french = self.language == "fr";
        let system_count = self.registry.system_ids().count();
        ui.horizontal_wrapped(|ui| {
            ui.vertical(|ui| {
                ui.label(
                    egui::RichText::new(if french {
                        "ENVIRONNEMENT"
                    } else {
                        "ENVIRONMENT"
                    })
                    .size(10.0)
                    .strong()
                    .color(self.accent_color),
                );
                ui.add_space(3.0);
                ui.heading(if french {
                    "Systèmes enregistrés"
                } else {
                    "Registered systems"
                });
                ui.label(if french {
                    "Capacités détectées auprès des cœurs, sans les supposer."
                } else {
                    "Capabilities reported by each core, never assumed by the interface."
                });
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                egui::Frame::group(ui.style())
                    .rounding(egui::Rounding::same(9.0))
                    .inner_margin(egui::Margin::symmetric(12.0, 8.0))
                    .show(ui, |ui| {
                        ui.label(format!(
                            "{} {}",
                            system_count,
                            if french { "système(s)" } else { "system(s)" }
                        ));
                    });
            });
        });
        ui.add_space(10.0);
        let system_ids: Vec<_> = self.registry.system_ids().collect();
        if system_ids.is_empty() {
            ui.label(if self.language == "fr" {
                "Aucun cœur n’est enregistré dans cette compilation. Les cœurs optionnels doivent être inclus ou installés avant de pouvoir lancer des jeux."
            } else {
                "No emulation core is registered in this build. Optional cores must be included or installed before games can be launched."
            });
            return;
        }
        for id in system_ids {
            if let Some(descriptor) = self.registry.registered_descriptor(id) {
                let caps = descriptor.capabilities;
                egui::Frame::group(ui.style())
                    .rounding(egui::Rounding::same(12.0))
                    .inner_margin(egui::Margin::same(14.0))
                    .show(ui, |ui| {
                        ui.heading(descriptor.display_name);
                        ui.small(id.as_str());
                        ui.separator();
                        ui.horizontal_wrapped(|ui| {
                            ui.label(format!(
                                "Content loading: {}",
                                capability_label(caps.content_loading)
                            ));
                            ui.label(format!("Input: {}", capability_label(caps.input)));
                            ui.label(format!(
                                "Video output: {}",
                                capability_label(caps.video_output)
                            ));
                            ui.label(format!(
                                "Audio output: {}",
                                capability_label(caps.audio_output)
                            ));
                            ui.label(format!(
                                "Save states: {}",
                                capability_label(caps.save_states)
                            ));
                            ui.label(format!(
                                "Persistent saves: {}",
                                capability_label(caps.persistent_saves)
                            ));
                        });
                    });
            }
        }
    }
}
