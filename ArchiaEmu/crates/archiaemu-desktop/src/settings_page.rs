//! settings page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_settings(&mut self, ui: &mut egui::Ui, context: &egui::Context) {
        egui::ScrollArea::vertical()
            .auto_shrink([false, false])
            .show(ui, |ui| {
                let french = self.language == "fr";
                let previous_dark_mode = self.dark_mode;
                let previous_ui_scale = self.ui_scale;
                let previous_accent_color = self.accent_color;
                ui.add_space(4.0);
                ui.horizontal(|ui| {
                    ui.label(
                        egui::RichText::new(if french { "APPARENCE" } else { "APPEARANCE" })
                            .small()
                            .strong()
                            .color(self.accent_color),
                    );
                    ui.label(
                        egui::RichText::new(" / ")
                            .small()
                            .color(ui.visuals().weak_text_color()),
                    );
                    ui.label(
                        egui::RichText::new(if french { "Préférences visuelles" } else { "Visual preferences" })
                            .small()
                            .color(ui.visuals().weak_text_color()),
                    );
                });
                ui.add_space(6.0);
                ui.heading(egui::RichText::new(if french { "Personnalisez votre espace" } else { "Make it feel like yours" }).size(27.0).strong());
                ui.label(if french {
                    "Ajustez le thème et les couleurs d’ArchiaEmu. Vos choix sont enregistrés localement."
                } else {
                    "Tune the look and feel of ArchiaEmu. Your preferences are saved locally."
                });
                ui.add_space(18.0);

                ui.columns(2, |columns| {
                    columns[0].vertical(|ui| {
                        egui::Frame::group(ui.style())
                            .fill(if self.dark_mode {
                                egui::Color32::from_rgb(25, 30, 42)
                            } else {
                                egui::Color32::from_rgb(255, 255, 255)
                            })
                            .stroke(egui::Stroke::new(1.0_f32, ui.visuals().widgets.noninteractive.bg_stroke.color))
                            .rounding(egui::Rounding::same(12.0))
                            .inner_margin(egui::Margin::same(16.0))
                            .show(ui, |ui| {
                                ui.label(egui::RichText::new(if french { "Thème" } else { "Theme" }).strong().size(16.0));
                                ui.label(egui::RichText::new(if french { "Choisissez votre ambiance" } else { "Choose your workspace mood" }).small().color(ui.visuals().weak_text_color()));
                                ui.add_space(10.0);
                                ui.horizontal(|ui| {
                                    let dark_selected = self.dark_mode;
                                    let light_selected = !self.dark_mode;
                                    let card_width = (ui.available_width() - 10.0) / 2.0;
                                    ui.vertical(|ui| {
                                        let (rect, response) = ui.allocate_exact_size(egui::vec2(card_width, 76.0), egui::Sense::click());
                                        let fill = egui::Color32::from_rgb(24, 29, 40);
                                        ui.painter().rect_filled(rect, egui::Rounding::same(8.0), fill);
                                        ui.painter().rect_stroke(rect, egui::Rounding::same(8.0), egui::Stroke::new(if dark_selected { 2.0_f32 } else { 1.0_f32 }, if dark_selected { self.accent_color } else { egui::Color32::from_rgb(65, 73, 91) }));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 8.0), egui::vec2(rect.width() * 0.34, 9.0)), egui::Rounding::same(3.0), egui::Color32::from_rgb(80, 91, 116));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 23.0), egui::vec2(rect.width() - 16.0, 5.0)), egui::Rounding::same(2.0), egui::Color32::from_rgb(55, 63, 80));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 34.0), egui::vec2(rect.width() * 0.56, 5.0)), egui::Rounding::same(2.0), self.accent_color);
                                        if response.clicked() { self.dark_mode = true; }
                                        ui.add_space(7.0);
                                        ui.horizontal(|ui| {
                                            ui.label(egui::RichText::new(if french { "Sombre" } else { "Dark" }).strong());
                                         });
                                    });
                                    ui.vertical(|ui| {
                                        let (rect, response) = ui.allocate_exact_size(egui::vec2(card_width, 76.0), egui::Sense::click());
                                        ui.painter().rect_filled(rect, egui::Rounding::same(8.0), egui::Color32::from_rgb(249, 250, 252));
                                        ui.painter().rect_stroke(rect, egui::Rounding::same(8.0), egui::Stroke::new(if light_selected { 2.0_f32 } else { 1.0_f32 }, if light_selected { self.accent_color } else { egui::Color32::from_rgb(220, 224, 232) }));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 8.0), egui::vec2(rect.width() * 0.34, 9.0)), egui::Rounding::same(3.0), egui::Color32::from_rgb(216, 221, 231));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 23.0), egui::vec2(rect.width() - 16.0, 5.0)), egui::Rounding::same(2.0), egui::Color32::from_rgb(226, 230, 237));
                                        ui.painter().rect_filled(egui::Rect::from_min_size(rect.min + egui::vec2(8.0, 34.0), egui::vec2(rect.width() * 0.56, 5.0)), egui::Rounding::same(2.0), self.accent_color);
                                        if response.clicked() { self.dark_mode = false; }
                                        ui.add_space(7.0);
                                        ui.horizontal(|ui| {
                                            ui.label(egui::RichText::new(if french { "Clair" } else { "Light" }).strong());
                                         });
                                    });
                                });
                                ui.add_space(6.0);
                                ui.label(egui::RichText::new(if french { "Le thème change les panneaux et les surfaces de l’application." } else { "Theme changes the app panels and surfaces." }).small().color(ui.visuals().weak_text_color()));
                            });
                    });
                    columns[1].vertical(|ui| {
                        egui::Frame::group(ui.style())
                            .rounding(egui::Rounding::same(12.0))
                            .inner_margin(egui::Margin::same(16.0))
                            .show(ui, |ui| {
                                ui.label(egui::RichText::new(if french { "Couleur d’accentuation" } else { "Accent color" }).strong().size(16.0));
                                ui.label(egui::RichText::new(if french { "Pour les actions et sélections" } else { "Used for actions and selections" }).small().color(ui.visuals().weak_text_color()));
                                ui.add_space(14.0);
                                let presets = [
                                    ("Blue", egui::Color32::from_rgb(75, 115, 210)),
                                    ("Violet", egui::Color32::from_rgb(133, 92, 230)),
                                    ("Teal", egui::Color32::from_rgb(20, 158, 145)),
                                    ("Green", egui::Color32::from_rgb(46, 160, 100)),
                                    ("Orange", egui::Color32::from_rgb(230, 126, 55)),
                                    ("Rose", egui::Color32::from_rgb(220, 80, 120)),
                                ];
                                ui.horizontal_wrapped(|ui| {
                                    for (name, color) in presets {
                                        let selected = self.accent_color == color;
                                        let (rect, response) = ui.allocate_exact_size(egui::vec2(34.0, 34.0), egui::Sense::click());
                                        ui.painter().circle_filled(rect.center(), 13.0, color);
                                        if selected {
                                            ui.painter().circle_stroke(rect.center(), 16.0, egui::Stroke::new(2.0_f32, color));
                                         }
                                        if response.clicked() { self.accent_color = color; }
                                        response.on_hover_text(name);
                                    }
                                });
                                ui.add_space(12.0);
                                ui.horizontal(|ui| {
                                    ui.label(if french { "Personnalisée" } else { "Custom" });
                                    ui.color_edit_button_srgba(&mut self.accent_color);
                                    ui.monospace(format!("#{:02X}{:02X}{:02X}", self.accent_color.r(), self.accent_color.g(), self.accent_color.b()));
                                });
                                ui.add_space(10.0);
                                ui.horizontal(|ui| {
                                    ui.label(if french { "Aperçu" } else { "Preview" });
                                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                                        let _ = ui.add(egui::Button::new(if french { "Action principale" } else { "Primary action" }).fill(self.accent_color).stroke(egui::Stroke::NONE));
                                    });
                                });
                                ui.add_space(3.0);
                                ui.label(egui::RichText::new(if french { "Les changements s’appliquent immédiatement." } else { "Changes apply immediately." }).small().color(ui.visuals().weak_text_color()));
                            });
                    });
                });

                ui.add_space(14.0);
                egui::Frame::group(ui.style())
                    .rounding(egui::Rounding::same(12.0))
                    .inner_margin(egui::Margin::same(16.0))
                    .show(ui, |ui| {
                        ui.horizontal(|ui| {
                            ui.vertical(|ui| {
                                ui.label(egui::RichText::new(if french { "Préférences d’affichage" } else { "Display preferences" }).strong().size(16.0));
                                ui.label(egui::RichText::new(if french { "Adaptez l’interface à votre écran." } else { "Make the interface comfortable on your screen." }).small().color(ui.visuals().weak_text_color()));
                            });
                            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                                ui.label(egui::RichText::new("Aa").size(21.0).strong().color(self.accent_color));
                            });
                        });
                        ui.add_space(12.0);
                        ui.columns(2, |columns| {
                            columns[0].vertical(|ui| {
                                ui.label(if french { "Langue de l’application" } else { "Application language" });
                                let previous_language = self.language.clone();
                                egui::ComboBox::from_id_source("application-language")
                                    .selected_text(if self.language == "fr" { "Français" } else { "English" })
                                    .width(ui.available_width())
                                    .show_ui(ui, |ui| {
                                        ui.selectable_value(&mut self.language, "fr".to_owned(), "Français");
                                        ui.selectable_value(&mut self.language, "en".to_owned(), "English");
                                    });
                                if previous_language != self.language {
                                    self.message = if self.language == "fr" { "Langue modifiée.".to_owned() } else { "Language changed.".to_owned() };
                                    self.save_preferences();
                                }
                            });
                            columns[1].vertical(|ui| {
                                ui.label(if french { "Échelle de l’interface" } else { "Interface scale" });
                                ui.add(egui::Slider::new(&mut self.ui_scale, 0.8..=1.4).suffix("x"));
                                ui.label(egui::RichText::new(if french { "Ajustable de 80 % à 140 %" } else { "Adjustable from 80% to 140%" }).small().color(ui.visuals().weak_text_color()));
                            });
                        });
                    });

                ui.add_space(12.0);
                ui.add_space(12.0);
                ui.heading(if self.language == "fr" {
                    "Paramètres · application"
                } else {
                    "Host settings · application"
                });
                egui::Frame::group(ui.style()).show(ui, |ui| {
                    ui.label(if self.language == "fr" { "ArchiaEmu est en cours de développement." } else { "ArchiaEmu is under active development." });
                    ui.label(if self.language == "fr" { "Le cœur Game Boy Color est expérimental ; le lancement d’une ROM ne garantit pas sa compatibilité." } else { "The Game Boy Color core is experimental; launching a ROM does not imply compatibility." });
                    ui.label(if self.language == "fr" { "Les chemins de la bibliothèque et les préférences visuelles sont enregistrés dans votre configuration utilisateur." } else { "Imported library paths and appearance preferences are saved in your user configuration." });
                    if ui.button(if self.language == "fr" { "Enregistrer les paramètres" } else { "Save settings now" }).clicked() && self.save_preferences() {
                        self.message = "Settings saved.".to_owned();
                    }
                });

                ui.add_space(12.0);
                ui.heading(if self.language == "fr" {
                    "Paramètres du cœur · indisponibles"
                } else {
                    "Core settings · not available yet"
                });
                egui::Frame::group(ui.style()).show(ui, |ui| {
                    ui.label(if self.language == "fr" { "Ces paramètres dépendront d’un futur cœur d’émulation et sont volontairement séparés de l’apparence de l’application." } else { "These settings belong to a future emulation core and are deliberately separate from host appearance preferences." });
                    ui.label(if self.language == "fr" { "Aucun réglage matériel propre au cœur n’est actuellement proposé." } else { "No core-specific hardware settings are currently exposed by this application." });
                    ui.add_enabled(false, egui::Button::new(if self.language == "fr" { "Les réglages apparaîtront lorsqu’un cœur les prendra en charge" } else { "Core settings will appear when a core supports them" }));
                });

                ui.add_space(12.0);
                ui.heading(if self.language == "fr" {
                    "Mises à jour de l’application"
                } else {
                    "Application updates"
                });
                egui::Frame::group(ui.style()).show(ui, |ui| {
                    ui.label(if self.language == "fr" { "ArchiaEmu vérifie automatiquement le manifeste officiel au démarrage; vous pouvez aussi relancer la vérification ici. Les modules sont téléchargés séparément." } else { "ArchiaEmu checks the official stable manifest at startup; you can also run a check here. Module downloads are independent." });
                    ui.horizontal_wrapped(|ui| {
                        let checking = self.update_check_receiver.is_some();
                        if ui
                            .add_enabled(!checking, egui::Button::new(if self.language == "fr" { "Rechercher des mises à jour" } else { "Check for updates" }))
                            .clicked()
                        {
                            self.start_update_check();
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
                            ui.hyperlink_to(if self.language == "fr" { "Notes de version" } else { "Release notes" }, &release_notes_url);
                        });
                        let current = option_env!("CARGO_PKG_VERSION").unwrap_or("0.0.0");
                        let update_available = update::is_newer_version(&version, current);
                        if update_available {
                            ui.label(format!("Update available: {current} to {version}"));
                            if let Some(download) = download {
                                let downloading = self.update_download_receiver.is_some();
                                if ui
                                    .add_enabled(
                                        !downloading,
                                        egui::Button::new(if self.language == "fr" { "Télécharger et vérifier la mise à jour" } else { "Download and verify update" }),
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
                                ui.small(if self.language == "fr" { "Aucun fichier de mise à jour installable n’est publié pour cette plateforme." } else { "No installable update artifact is currently published for this platform." });
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
                        ui.small(if self.language == "fr" { "Le fichier est vérifié, mais n’a pas été installé ni exécuté." } else { "The file is verified but has not been installed or executed." });
                    }
                    ui.small(if self.language == "fr" { "L’installation automatique et le retour arrière ne sont pas encore activés." } else { "Automatic installation and rollback are not enabled yet." });
                });

                ui.add_space(12.0);
                ui.heading(if self.language == "fr" {
                    "Diagnostic"
                } else {
                    "Diagnostics"
                });
                ui.label(if self.language == "fr" {
                    "Informations locales pour diagnostiquer les problèmes de démarrage et de session."
                } else {
                    "Local runtime information to help troubleshoot startup and session issues."
                });
                egui::Frame::group(ui.style()).show(ui, |ui| {
                    ui.horizontal_wrapped(|ui| {
                        ui.label(if self.language == "fr" { "Version de l’application :" } else { "Application version:" });
                        ui.monospace(option_env!("CARGO_PKG_VERSION").unwrap_or("unknown"));
                        ui.separator();
                        ui.label(if self.language == "fr" { "Plateforme :" } else { "Platform:" });
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
                        ui.label(if self.language == "fr" { "Session active :" } else { "Active session:" });
                        ui.monospace(
                            self.controller
                                .active_session()
                                .map(|id| id.get().to_string())
                                .unwrap_or_else(|| "none".to_owned()),
                        );
                    });
                    ui.label(if self.language == "fr" { "Fichier de configuration :" } else { "Configuration file:" });
                    ui.monospace(
                        self.config_path
                            .as_deref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "unavailable".to_owned()),
                    );
                    ui.label(if self.language == "fr" { "État des médias :" } else { "Media status:" });
                    ui.label(&self.media_status);
                    ui.label(if self.language == "fr" { "État des mises à jour :" } else { "Update status:" });
                    ui.label(&self.update_status);
                    ui.label(if self.language == "fr" { "Mise à jour préparée :" } else { "Staged update:" });
                    ui.label(
                        self.staged_update_path
                            .as_deref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "none".to_owned()),
                    );
                    ui.label(if self.language == "fr" { "Dernier message de l’application :" } else { "Latest application message:" });
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
                // Persist visual preferences as soon as the user changes them.
                // Comparing against this frame snapshot avoids writing the config every frame.
                if previous_dark_mode != self.dark_mode
                    || (previous_ui_scale - self.ui_scale).abs() > f32::EPSILON
                    || previous_accent_color != self.accent_color
                {
                    self.save_preferences();
                }
            });
    }
}
