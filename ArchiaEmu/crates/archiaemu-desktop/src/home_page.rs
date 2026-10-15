//! home page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_home(&mut self, ui: &mut egui::Ui) {
        ui.heading(if self.language == "fr" { "Bienvenue" } else { "Welcome back" });
        ui.label(
            "Manage local content, inspect supported metadata, and monitor application sessions.",
        );
        ui.add_space(8.0);

        let card_columns = if ui.available_width() < 560.0 {
            1
        } else if ui.available_width() < 820.0 {
            2
        } else {
            3
        };
        ui.columns(card_columns, |columns| {
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
            for (index, (title, value, detail)) in cards.into_iter().enumerate() {
                let column = &mut columns[index % card_columns];
                egui::Frame::group(column.style()).show(column, |ui| {
                    ui.label(title);
                    ui.heading(value);
                    ui.small(detail);
                });
            }
        });

        ui.add_space(14.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.heading(if self.language == "fr" { "Importer du contenu local" } else { "Import local content" });
            ui.label(if self.language == "fr" { "Importez un fichier Game Boy ou Game Boy Color local pour examiner ses métadonnées. Le jeu complet n’est pas encore implémenté." } else { "Import a local Game Boy or Game Boy Color file to inspect its cartridge metadata. Full gameplay is not implemented yet." });
            ui.add_space(8.0);
            ui.horizontal_wrapped(|ui| {
                ui.add(
                    egui::TextEdit::singleline(&mut self.content_path)
                        .hint_text("Local path to a .gb or .gbc file")
                        .desired_width(ui.available_width().min(560.0)),
                );
                if ui.button(if self.language == "fr" { "Parcourir…" } else { "Browse…" }).clicked() {
                    self.choose_content_file();
                }
                if ui.button(if self.language == "fr" { "Importer" } else { "Import" }).clicked() {
                    self.import_content();
                }
                if ui.button(if self.language == "fr" { "Lancer" } else { "Launch" }).clicked() {
                    self.launch_content();
                }
            });
        });

        ui.add_space(14.0);
        ui.horizontal_wrapped(|ui| {
            ui.heading(if self.language == "fr" { "Imports récents" } else { "Recently imported" });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.link("View library →").clicked() {
                    self.navigate_to(DesktopPage::Library);
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
                ui.label(if self.language == "fr" { "Votre bibliothèque est vide." } else { "Your library is empty." });
                ui.label(if self.language == "fr" { "Utilisez Parcourir ou collez le chemin d’une ROM ci-dessus." } else { "Use Browse or paste a ROM path above to get started." });
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
                                    if ui.button(if self.language == "fr" { "Lancer" } else { "Launch" }).clicked() {
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
            ui.horizontal_wrapped(|ui| {
                ui.vertical(|ui| {
                    ui.heading(if self.language == "fr" { "État de l’émulation" } else { "Emulation status" });
                    ui.label(if self.language == "fr" { "Prototype de l’application : bibliothèque, préférences et sessions sont disponibles ; aucun cœur de console jouable n’est encore implémenté." } else { "Desktop application prototype: library, preferences, and session management are available; no playable console core is implemented yet." });
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.label(if self.language == "fr" { "Expérimental" } else { "Experimental" });
                });
            });
        });
    }
}
