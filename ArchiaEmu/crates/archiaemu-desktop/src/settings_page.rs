//! settings page for the ArchiaEmu desktop frontend.

use super::*;

impl DesktopApp {
    pub(super) fn show_settings(&mut self, ui: &mut egui::Ui, context: &egui::Context) {
        match self.settings_section {
            SettingsSection::Appearance => self.show_appearance(ui),
            SettingsSection::Application => self.show_application_settings(ui, context),
        }
    }

    fn show_application_settings(&mut self, ui: &mut egui::Ui, context: &egui::Context) {
        ui.label(egui::RichText::new("Application").size(30.0).strong());
        ui.label(
            egui::RichText::new(if self.language == "fr" {
                "Informations, mises à jour et diagnostic de l’application."
            } else {
                "Application information, updates and diagnostics."
            })
            .color(ui.visuals().weak_text_color()),
        );
        ui.add_space(8.0);
        ui.separator();

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
                    ui.label(if self.language == "fr" { format!("Version du manifeste : {version}") } else { format!("Manifest version: {version}") });
                    ui.hyperlink_to(if self.language == "fr" { "Notes de version" } else { "Release notes" }, &release_notes_url);
                });
                let current = option_env!("CARGO_PKG_VERSION").unwrap_or("0.0.0");
                let update_available = update::is_newer_version(&version, current);
                if update_available {
                    ui.label(if self.language == "fr" { format!("Mise à jour disponible : {current} → {version}") } else { format!("Update available: {current} to {version}") });
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
                            self.update_status = if self.language == "fr" {
                                format!("Téléchargement de la version {version} dans la zone temporaire…")
                            } else {
                                format!("Downloading version {version} to a temporary staging area…")
                            };
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
                    ui.label(if self.language == "fr" {
                        format!("La version installée ({current}) est plus récente que le manifeste stable ({version}).")
                    } else {
                        format!("Installed version ({current}) is newer than the stable manifest ({version}).")
                    });
                } else {
                    ui.label(if self.language == "fr" { format!("ArchiaEmu est à jour ({current}).") } else { format!("ArchiaEmu is up to date ({current}).") });
                }
            }
            if let Some(path) = self.staged_update_path.as_ref() {
                ui.label(if self.language == "fr" { format!("Fichier vérifié préparé : {}", path.display()) } else { format!("Verified staged file: {}", path.display()) });
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
                ui.label(if self.language == "fr" { format!("Éléments de la bibliothèque : {}", self.controller.library().len()) } else { format!("Library items: {}", self.controller.library().len()) });
                ui.separator();
                ui.label(if self.language == "fr" { format!("Sessions ouvertes : {}", self.controller.session_summaries().len()) } else { format!("Open sessions: {}", self.controller.session_summaries().len()) });
                ui.separator();
                ui.label(if self.language == "fr" { format!("Erreurs d’exécution : {}", self.last_tick_errors.len()) } else { format!("Execution errors: {}", self.last_tick_errors.len()) });
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
                ui.colored_label(egui::Color32::LIGHT_RED, if self.language == "fr" { "Erreurs récentes des sessions" } else { "Recent session errors" });
                for (session_id, error) in self.last_tick_errors.iter().take(5) {
                    ui.label(if self.language == "fr" { format!("Session {}: {error}", session_id.get()) } else { format!("Session {} : {error}", session_id.get()) });
                }
            }
            if ui.button(if self.language == "fr" { "Copier le diagnostic" } else { "Copy diagnostics to clipboard" }).clicked() {
                let recent_errors = self
                    .last_tick_errors
                    .iter()
                    .take(5)
                    .map(|(session_id, error)| {
                        if self.language == "fr" {
                            format!("Session {} : {error}", session_id.get())
                        } else {
                            format!("Session {}: {error}", session_id.get())
                        }
                    })
                    .collect::<Vec<_>>()
                    .join("\n");
                let active_session = self
                    .controller
                    .active_session()
                    .map(|id| id.get().to_string())
                    .unwrap_or_else(|| "none".to_owned());
                let report = if self.language == "fr" {
                    format!(
                        "ArchiaEmu {}\nPlateforme : {} / {}\nÉléments de la bibliothèque : {}\nSessions ouvertes : {}\nSession active : {}\nErreurs d’exécution : {}\nConfiguration : {}\nÉtat des médias : {}\nÉtat des mises à jour : {}\nMise à jour préparée : {}\nDernier message : {}\nErreurs récentes :\n{}",
                        option_env!("CARGO_PKG_VERSION").unwrap_or("inconnue"),
                        std::env::consts::OS,
                        std::env::consts::ARCH,
                        self.controller.library().len(),
                        self.controller.session_summaries().len(),
                        active_session,
                        self.last_tick_errors.len(),
                        self.config_path.as_deref().map(|path| path.display().to_string()).unwrap_or_else(|| "indisponible".to_owned()),
                        self.media_status,
                        self.update_status,
                        self.staged_update_path.as_deref().map(|path| path.display().to_string()).unwrap_or_else(|| "aucune".to_owned()),
                        self.message,
                        recent_errors
                    )
                } else {
                    format!(
                        "ArchiaEmu {}\nPlatform: {} / {}\nLibrary items: {}\nOpen sessions: {}\nActive session: {}\nExecution errors: {}\nConfiguration: {}\nMedia status: {}\nUpdate status: {}\nStaged update: {}\nLatest message: {}\nRecent errors:\n{}",
                        option_env!("CARGO_PKG_VERSION").unwrap_or("unknown"),
                        std::env::consts::OS,
                        std::env::consts::ARCH,
                        self.controller.library().len(),
                        self.controller.session_summaries().len(),
                        active_session,
                        self.last_tick_errors.len(),
                        self.config_path.as_deref().map(|path| path.display().to_string()).unwrap_or_else(|| "unavailable".to_owned()),
                        self.media_status,
                        self.update_status,
                        self.staged_update_path.as_deref().map(|path| path.display().to_string()).unwrap_or_else(|| "none".to_owned()),
                        self.message,
                        recent_errors
                    )
                };
                ui.output_mut(|output| output.copied_text = report);
                self.message = if self.language == "fr" {
                    "Diagnostic copié dans le presse-papiers.".to_owned()
                } else {
                    "Diagnostics copied to clipboard.".to_owned()
                };
            }
        });
    }
}
