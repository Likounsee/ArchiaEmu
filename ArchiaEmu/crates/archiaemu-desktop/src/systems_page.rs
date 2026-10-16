//! Systems and module management page for the ArchiaEmu desktop frontend.

use super::*;
use archiaemu_core::module_management::ModuleId;
use archiaemu_core::module_state_store::{default_user_state_path, ModuleStateStore};

impl DesktopApp {
    pub(super) fn show_systems(&mut self, ui: &mut egui::Ui) {
        let french = self.language == "fr";
        let system_ids: Vec<_> = self.registry.system_ids().collect();
        let system_count = system_ids.len();
        let capability_label = |enabled| {
            if enabled {
                if french {
                    "Disponible"
                } else {
                    "Available"
                }
            } else if french {
                "Non implémenté"
            } else {
                "Not implemented"
            }
        };

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
                    "Systèmes et modules"
                } else {
                    "Systems and modules"
                });
                ui.label(if french {
                    "Disponibilité issue des cœurs enregistrés ; sélection conservée dans votre profil."
                } else {
                    "Availability comes from registered cores; selection is persisted in your profile."
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
                            if french {
                                if system_count == 1 {
                                    "système"
                                } else {
                                    "systèmes"
                                }
                            } else if system_count == 1 {
                                "system"
                            } else {
                                "systems"
                            }
                        ));
                    });
            });
        });
        ui.add_space(10.0);

        let state_path = match default_user_state_path() {
            Ok(path) => path,
            Err(error) => {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    if french {
                        format!("État des modules indisponible : {error}")
                    } else {
                        format!("Module state unavailable: {error}")
                    },
                );
                return;
            }
        };
        let store = ModuleStateStore::new(state_path);
        let mut state = match store.load() {
            Ok(state) => state,
            Err(error) => {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    if french {
                        format!("Impossible de lire l’état des modules : {error}")
                    } else {
                        format!("Could not load module state: {error}")
                    },
                );
                ui.label(if french {
                    "Le fichier d’état est conservé pour permettre sa récupération ; aucune sélection n’a été écrasée."
                } else {
                    "The state file was preserved for recovery; no selection was overwritten."
                });
                return;
            }
        };

        ui.heading(if french {
            "Sélection des modules"
        } else {
            "Module selection"
        });
        ui.label(if french {
            "Cette sélection enregistre votre préférence. Elle ne télécharge ni n’installe de paquet."
        } else {
            "This records your preference only. It does not download or install a package."
        });

        let mut selected = state.selected_modules().to_vec();
        let mut changed = false;
        if system_ids.is_empty() {
            ui.label(if french {
                "Aucun cœur n’est enregistré dans cette compilation."
            } else {
                "No emulation core is registered in this build."
            });
        }

        for id in &system_ids {
            let Ok(module_id) = ModuleId::new(id.as_str().to_owned()) else {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    format!(
                        "{}: {}",
                        if french {
                            "Identifiant non valide"
                        } else {
                            "Invalid module ID"
                        },
                        id.as_str()
                    ),
                );
                continue;
            };
            let available = self.registry.registered_descriptor(*id);
            let mut is_selected = selected.contains(&module_id);
            ui.horizontal(|ui| {
                if ui.checkbox(&mut is_selected, id.as_str()).changed() {
                    changed = true;
                    if is_selected {
                        if !selected.contains(&module_id) {
                            selected.push(module_id.clone());
                        }
                    } else {
                        selected.retain(|selected_id| selected_id != &module_id);
                    }
                }
                ui.label(if available.is_some() {
                    if french {
                        "Disponible dans cette compilation"
                    } else {
                        "Available in this build"
                    }
                } else if french {
                    "Non disponible"
                } else {
                    "Unavailable"
                });
            });
            if let Some(descriptor) = available {
                let caps = descriptor.capabilities;
                ui.indent(id.as_str(), |ui| {
                    ui.horizontal_wrapped(|ui| {
                        for (label, enabled) in [
                            (
                                if french { "Contenu" } else { "Content" },
                                caps.content_loading,
                            ),
                            (if french { "Entrées" } else { "Input" }, caps.input),
                            (if french { "Vidéo" } else { "Video" }, caps.video_output),
                            (if french { "Audio" } else { "Audio" }, caps.audio_output),
                            (
                                if french {
                                    "États de sauvegarde"
                                } else {
                                    "Save states"
                                },
                                caps.save_states,
                            ),
                            (
                                if french {
                                    "Sauvegardes persistantes"
                                } else {
                                    "Persistent saves"
                                },
                                caps.persistent_saves,
                            ),
                        ] {
                            ui.label(format!("{label}: {}", capability_label(enabled)));
                        }
                    });
                });
            }
        }

        if changed {
            state.set_selection(selected);
            match store.save(&state) {
                Ok(()) => {
                    self.message = if french {
                        "Sélection des modules enregistrée.".to_owned()
                    } else {
                        "Module selection saved.".to_owned()
                    };
                }
                Err(error) => {
                    self.message = if french {
                        format!("Échec de l’enregistrement de la sélection : {error}")
                    } else {
                        format!("Could not save module selection: {error}")
                    };
                }
            }
        }

        ui.add_space(12.0);
        ui.separator();
        ui.heading(if french {
            "Modules installés"
        } else {
            "Installed modules"
        });
        if state.installed_modules().is_empty() {
            ui.label(if french {
                "Aucun paquet de module externe n’est enregistré comme installé."
            } else {
                "No external module package is recorded as installed."
            });
        } else {
            for installed in state.installed_modules() {
                ui.horizontal(|ui| {
                    ui.label(egui::RichText::new(installed.id.as_str()).strong());
                    ui.label("Version");
                    ui.monospace(&installed.version);
                });
            }
        }
        ui.small(if french {
            "Les cœurs compilés et les paquets installés sont suivis séparément."
        } else {
            "Built-in cores and installed packages are tracked separately."
        });
    }
}
