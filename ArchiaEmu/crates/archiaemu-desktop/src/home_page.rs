//! home page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_home(&mut self, ui: &mut egui::Ui) {
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
                    self.controller.session_count().to_string(),
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
}
