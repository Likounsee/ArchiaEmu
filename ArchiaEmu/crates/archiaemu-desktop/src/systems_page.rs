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
                "Indisponible"
            } else {
                "Unavailable"
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
                    if french { format!("État des modules indisponible : {error}") } else { format!("Module state unavailable: {error}") },
                );
                return;
            }
        };
        let store = ModuleStateStore::new(state_path);
        let state = match store.load() {
            Ok(state) => archiaemu_app::view_state::ViewState::Ready(state),
            Err(error) => archiaemu_app::view_state::ViewState::Error(error.to_string()),
        };
        let mut state = match state {
            archiaemu_app::view_state::ViewState::Ready(state) => state,
            archiaemu_app::view_state::ViewState::Error(error) => {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    if french { format!("Impossible de lire l’état des modules : {error}") } else { format!("Could not load module state: {error}") },
                );
                ui.label(if french {
                    "Le fichier d’état est conservé pour permettre sa récupération ; aucune sélection n’a été écrasée."
                } else {
                    "The state file was preserved for recovery; no selection was overwritten."
                });
                return;
            }
            archiaemu_app::view_state::ViewState::Loading
            | archiaemu_app::view_state::ViewState::Empty
            | archiaemu_app::view_state::ViewState::Unavailable(_) => {
                unreachable!("loading a module state file is synchronous and either returns data or an error")
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

        ui.add_space(8.0);
        ui.heading(if french { "Modules disponibles" } else { "Available modules" });
        ui.label(if french {
            "Activez uniquement les modules que vous souhaitez utiliser. Les modules absents de cette compilation restent désactivés."
        } else {
            "Enable only the modules you want to use. Modules not bundled in this build remain unavailable."
        });
        ui.add_space(4.0);

        for id in &system_ids {
            let Ok(module_id) = ModuleId::new(id.as_str().to_owned()) else {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    format!(
                        "{}: {}",
                        if french { "Identifiant non valide" } else { "Invalid module ID" },
                        id.as_str()
                    ),
                );
                continue;
            };
            let available = self.registry.registered_descriptor(*id);
            let installed_version = state.installed_version(&module_id).map(str::to_owned);
            let mut is_selected = selected.contains(&module_id);
            egui::Frame::group(ui.style())
                .rounding(egui::Rounding::same(10.0))
                .inner_margin(egui::Margin::same(12.0))
                .show(ui, |ui| {
                    ui.horizontal_wrapped(|ui| {
                        ui.vertical(|ui| {
                            ui.heading(egui::RichText::new(id.as_str()).strong());
                            ui.label(if let Some(version) = installed_version.as_deref() {
                                if french {
                                    format!("Installé · version {version}")
                                } else {
                                    format!("Installed · version {version}")
                                }
                            } else if available.is_some() {
                                if french {
                                    "Disponible dans cette compilation".to_owned()
                                } else {
                                    "Available in this build".to_owned()
                                }
                            } else if french {
                                "Non disponible".to_owned()
                            } else {
                                "Unavailable".to_owned()
                            });
                        });
                        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                            if ui.checkbox(&mut is_selected, if french { "Activé" } else { "Enabled" }).changed() {
                                changed = true;
                                if is_selected {
                                    if !selected.contains(&module_id) {
                                        selected.push(module_id.clone());
                                    }
                                } else {
                                    selected.retain(|selected_id| selected_id != &module_id);
                                }
                            }
                        });
                    });
                    if let Some(descriptor) = available {
                        let caps = descriptor.capabilities;
                        ui.add_space(8.0);
                        ui.separator();
                        ui.label(egui::RichText::new(if french { "CAPACITÉS" } else { "CAPABILITIES" }).small().strong());
                        ui.add_space(4.0);
                        ui.horizontal_wrapped(|ui| {
                            for (label, enabled) in [
                                (
                                    if french { "Contenu" } else { "Content" },
                                    caps.content_loading,
                                ),
                                (if french { "Entrées" } else { "Input" }, caps.input),
                                (if french { "Vidéo" } else { "Video" }, caps.video_output),
                                ("Audio", caps.audio_output),
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
                                let color = if enabled {
                                    egui::Color32::from_rgb(23, 178, 106)
                                } else {
                                    ui.visuals().weak_text_color()
                                };
                                egui::Frame::none()
                                    .fill(ui.visuals().faint_bg_color)
                                    .rounding(egui::Rounding::same(6.0))
                                    .inner_margin(egui::Margin::symmetric(8.0, 4.0))
                                    .show(ui, |ui| {
                                        ui.horizontal(|ui| {
                                            ui.label(
                                                egui::RichText::new(if enabled {
                                                    "●"
                                                } else {
                                                    "○"
                                                })
                                                .color(color),
                                            );
                                            ui.label(format!(
                                                "{label}: {}",
                                                capability_label(enabled)
                                            ));
                                        });
                                    });
                            }
                        });
                    }
                });
        }

        // Keep persisted selections visible even when this build does not ship
        // the corresponding core. Hiding them would make old preferences look lost.
        let unavailable_selected: Vec<_> = selected
            .iter()
            .filter(|selected_id| {
                !system_ids
                    .iter()
                    .any(|system_id| system_id.as_str() == selected_id.as_str())
            })
            .cloned()
            .collect();
        if !unavailable_selected.is_empty() {
            ui.add_space(4.0);
            ui.label(if french {
                "Sélections conservées mais indisponibles dans cette compilation :"
            } else {
                "Selections retained but unavailable in this build:"
            });
            let mut deselected_unavailable = Vec::new();
            for selected_id in unavailable_selected {
                let mut keep_selected = true;
                ui.horizontal(|ui| {
                    if ui
                        .checkbox(&mut keep_selected, selected_id.as_str())
                        .changed()
                        && !keep_selected
                    {
                        deselected_unavailable.push(selected_id.clone());
                    }
                    ui.colored_label(
                        egui::Color32::GRAY,
                        if french {
                            "Cœur absent"
                        } else {
                            "Core not bundled"
                        },
                    );
                });
            }
            if !deselected_unavailable.is_empty() {
                selected.retain(|id| !deselected_unavailable.contains(id));
                changed = true;
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
                    ui.label(if french { "Version" } else { "Version" });
                    ui.monospace(&installed.version);
                    let bundled = system_ids
                        .iter()
                        .any(|system_id| system_id.as_str() == installed.id.as_str());
                    ui.label(if bundled {
                        if french {
                            "Cœur également intégré"
                        } else {
                            "Core also bundled"
                        }
                    } else if french {
                        "Paquet externe"
                    } else {
                        "External package"
                    });
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
