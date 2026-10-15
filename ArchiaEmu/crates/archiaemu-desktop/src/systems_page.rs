//! systems page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_systems(&mut self, ui: &mut egui::Ui) {
        ui.heading(if self.language == "fr" {
            "Systèmes enregistrés"
        } else {
            "Registered systems"
        });
        ui.label(if self.language == "fr" {
            "Les capacités viennent de la description de chaque cœur, pas de l’interface."
        } else {
            "Capabilities are reported from each core descriptor, not inferred from the UI."
        });
        ui.add_space(8.0);
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
