//! Polished overview dashboard for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_home(&mut self, ui: &mut egui::Ui) {
        let french = self.language == "fr";
        let accent = self.accent_color;
        let panel = ui.visuals().faint_bg_color;
        let border = ui.visuals().widgets.noninteractive.bg_stroke;
        let text = ui.visuals().text_color();
        let muted = ui.visuals().weak_text_color();

        // Hero
        egui::Frame::none()
            .fill(if self.dark_mode {
                egui::Color32::from_rgb(24, 31, 49)
            } else {
                egui::Color32::from_rgb(235, 240, 252)
            })
            .rounding(egui::Rounding::same(16.0))
            .inner_margin(egui::Margin::same(24.0))
            .stroke(egui::Stroke::new(1.0, accent.gamma_multiply(0.45)))
            .show(ui, |ui| {
                ui.set_min_height(172.0);
                ui.horizontal_wrapped(|ui| {
                    ui.vertical(|ui| {
                        ui.label(
                            egui::RichText::new(if french {
                                "VOTRE ESPACE D’ÉMULATION"
                            } else {
                                "YOUR EMULATION WORKSPACE"
                            })
                            .size(11.0)
                            .strong()
                            .color(accent),
                        );
                        ui.add_space(8.0);
                        ui.heading(
                            egui::RichText::new(if french {
                                "Vos jeux. Un seul endroit."
                            } else {
                                "Your games. One place."
                            })
                            .size(28.0)
                            .strong(),
                        );
                        ui.add_space(4.0);
                        ui.label(if french {
                            "Organisez votre bibliothèque et retrouvez vos sessions en un instant."
                        } else {
                            "Organize your library and get back to your sessions in seconds."
                        });
                        ui.add_space(16.0);
                        ui.horizontal_wrapped(|ui| {
                            if ui
                                .add(
                                    egui::Button::new(
                                        egui::RichText::new(if french {
                                            "＋  Importer des jeux"
                                        } else {
                                            "＋  Import games"
                                        })
                                        .strong(),
                                    )
                                    .fill(accent)
                                    .min_size(egui::vec2(150.0, 38.0)),
                                )
                                .clicked()
                            {
                                self.choose_content_file();
                            }
                            if ui
                                .add(
                                    egui::Button::new(if french {
                                        "Ouvrir la bibliothèque  →"
                                    } else {
                                        "Open library  →"
                                    })
                                    .min_size(egui::vec2(150.0, 38.0)),
                                )
                                .clicked()
                            {
                                self.navigate_to(DesktopPage::Library);
                            }
                        });
                    });
                });
            });

        ui.add_space(16.0);

        // At-a-glance metrics
        let metrics = [
            (
                if french { "BIBLIOTHÈQUE" } else { "LIBRARY" },
                self.controller.library().len().to_string(),
                if french { "jeux importés" } else { "imported items" },
                egui::Color32::from_rgb(111, 155, 255),
            ),
            (
                if french { "SESSIONS" } else { "SESSIONS" },
                self.controller.session_count().to_string(),
                if french { "sessions ouvertes" } else { "open sessions" },
                egui::Color32::from_rgb(98, 205, 164),
            ),
            (
                if french { "SYSTÈMES" } else { "SYSTEMS" },
                self.registry.system_ids().count().to_string(),
                if french { "cœurs enregistrés" } else { "registered cores" },
                egui::Color32::from_rgb(190, 145, 255),
            ),
        ];
        let columns = if ui.available_width() < 620.0 { 1 } else { 3 };
        ui.columns(columns, |cols| {
            for (index, (label, value, detail, tint)) in metrics.into_iter().enumerate() {
                let card_ui = &mut cols[index % columns];
                egui::Frame::none()
                    .fill(panel)
                    .rounding(egui::Rounding::same(12.0))
                    .inner_margin(egui::Margin::same(16.0))
                    .stroke(border)
                    .show(card_ui, |ui| {
                        ui.horizontal(|ui| {
                            ui.add(
                                egui::Label::new(
                                    egui::RichText::new("●").size(12.0).color(tint),
                                ),
                            );
                            ui.label(
                                egui::RichText::new(label)
                                    .size(10.0)
                                    .strong()
                                    .color(muted),
                            );
                        });
                        ui.add_space(6.0);
                        ui.heading(egui::RichText::new(value).size(30.0).strong().color(text));
                        ui.label(egui::RichText::new(detail).size(12.0).color(muted));
                    });
            }
        });

        ui.add_space(18.0);
        ui.horizontal_wrapped(|ui| {
            ui.heading(if french { "Reprendre" } else { "Pick up where you left off" });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.link(if french { "Toute la bibliothèque  →" } else { "View library  →" }).clicked() {
                    self.navigate_to(DesktopPage::Library);
                }
            });
        });
        ui.add_space(8.0);

        let entries: Vec<_> = self
            .controller
            .library()
            .entries()
            .map(|entry| (
                entry.title().to_owned(),
                entry.path().to_path_buf(),
                entry.size_bytes(),
            ))
            .collect();

        if entries.is_empty() {
            egui::Frame::none()
                .fill(panel)
                .rounding(egui::Rounding::same(12.0))
                .inner_margin(egui::Margin::same(20.0))
                .stroke(border)
                .show(ui, |ui| {
                    ui.horizontal(|ui| {
                        ui.label(egui::RichText::new("＋").size(28.0).color(accent));
                        ui.vertical(|ui| {
                            ui.label(egui::RichText::new(if french {
                                "Votre bibliothèque commence ici"
                            } else {
                                "Your library starts here"
                            }).strong().size(16.0));
                            ui.label(if french {
                                "Importez une ROM .gb ou .gbc pour l’ajouter à votre collection."
                            } else {
                                "Import a .gb or .gbc ROM to add it to your collection."
                            });
                            ui.add_space(6.0);
                            if ui.button(if french { "Parcourir les fichiers…" } else { "Browse files…" }).clicked() {
                                self.choose_content_file();
                            }
                        });
                    });
                });
        } else {
            egui::Frame::none()
                .fill(panel)
                .rounding(egui::Rounding::same(12.0))
                .inner_margin(egui::Margin::same(8.0))
                .stroke(border)
                .show(ui, |ui| {
                    for (title, path, size) in entries.iter().take(5) {
                        ui.horizontal(|ui| {
                            egui::Frame::none()
                                .fill(accent.gamma_multiply(0.18))
                                .rounding(egui::Rounding::same(8.0))
                                .inner_margin(egui::Margin::same(10.0))
                                .show(ui, |ui| {
                                    ui.label(egui::RichText::new("GB").strong().color(accent));
                                });
                            ui.add_space(4.0);
                            ui.vertical(|ui| {
                                ui.label(egui::RichText::new(title).strong());
                                ui.label(egui::RichText::new(format!(
                                    "{}  ·  {}",
                                    path.file_name().and_then(|name| name.to_str()).unwrap_or("ROM"),
                                    format_size(*size)
                                )).size(12.0).color(muted));
                            });
                            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                                if ui.button(if french { "Lancer  →" } else { "Launch  →" }).clicked() {
                                    self.content_path = path.display().to_string();
                                    self.selected_content = Some(path.clone());
                                    self.launch_content();
                                }
                            });
                        });
                        ui.add_space(4.0);
                        ui.separator();
                    }
                });
        }

        ui.add_space(16.0);
        egui::Frame::none()
            .fill(panel)
            .rounding(egui::Rounding::same(12.0))
            .inner_margin(egui::Margin::same(16.0))
            .stroke(border)
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.vertical(|ui| {
                        ui.label(egui::RichText::new(if french { "ÉTAT DU PROJET" } else { "PROJECT STATUS" })
                            .size(10.0).strong().color(muted));
                        ui.add_space(4.0);
                        ui.label(egui::RichText::new(if french {
                            "Développement en cours"
                        } else {
                            "Actively in development"
                        }).strong().size(15.0));
                        ui.label(if french {
                            "La bibliothèque et la gestion des sessions sont disponibles. La compatibilité dépend des cœurs intégrés."
                        } else {
                            "Library and session management are available. Compatibility depends on the cores included in this build."
                        });
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        egui::Frame::none()
                            .fill(accent.gamma_multiply(0.16))
                            .rounding(egui::Rounding::same(20.0))
                            .inner_margin(egui::Margin { left: 10.0, right: 10.0, top: 6.0, bottom: 6.0 })
                            .show(ui, |ui| {
                                ui.label(egui::RichText::new(if french { "EXPÉRIMENTAL" } else { "EXPERIMENTAL" })
                                    .size(10.0).strong().color(accent));
                            });
                    });
                });
            });
    }
}
