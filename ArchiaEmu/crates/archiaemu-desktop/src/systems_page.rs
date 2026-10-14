//! systems page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_systems(&mut self, ui: &mut egui::Ui) {
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
