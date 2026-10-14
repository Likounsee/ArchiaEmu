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
}
