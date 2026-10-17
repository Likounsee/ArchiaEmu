//! Systems and module management page for the PeaceEmu desktop frontend.
//!
//! Layout: header with a systems counter, three summary cards, one card per
//! registered core (tile, status badge, on/off switch, capability badges),
//! then the retained-but-missing selections and the installed packages.
//! All colours come from the shared [`theme::Palette`].

use super::*;
use emulation_core::module_management::ModuleId;
use emulation_core::module_state_store::{default_user_state_path, ModuleStateStore};

/// A pill-shaped on/off switch. Returns `true` on the frame it is toggled.
fn toggle_switch(ui: &mut egui::Ui, palette: &theme::Palette, on: &mut bool) -> bool {
    let (rect, response) = ui.allocate_exact_size(egui::vec2(40.0, 22.0), egui::Sense::click());
    let mut toggled = false;
    if response.clicked() {
        *on = !*on;
        toggled = true;
    }
    let openness = ui
        .ctx()
        .animate_bool_with_time(response.id, *on, 0.12);
    let radius = rect.height() / 2.0;
    let off_track = if palette.dark {
        palette.border
    } else {
        egui::Color32::from_rgb(212, 212, 216)
    };
    let track = if *on { palette.accent } else { off_track };
    ui.painter()
        .rect_filled(rect, egui::Rounding::same(radius), track);
    let knob_x = egui::lerp((rect.left() + radius)..=(rect.right() - radius), openness);
    ui.painter().circle_filled(
        egui::pos2(knob_x, rect.center().y),
        radius - 3.0,
        egui::Color32::WHITE,
    );
    let _ = response.on_hover_cursor(egui::CursorIcon::PointingHand);
    toggled
}

/// Square tile showing the first two letters of a core identifier.
fn module_tile(ui: &mut egui::Ui, palette: &theme::Palette, name: &str) {
    let (rect, _) = ui.allocate_exact_size(egui::vec2(44.0, 44.0), egui::Sense::hover());
    ui.painter()
        .rect_filled(rect, egui::Rounding::same(10.0), palette.accent_subtle);
    let initials: String = name
        .chars()
        .filter(|character| character.is_alphanumeric())
        .take(2)
        .collect::<String>()
        .to_uppercase();
    ui.painter().text(
        rect.center(),
        egui::Align2::CENTER_CENTER,
        initials,
        egui::FontId::proportional(16.0),
        palette.text,
    );
}

/// A small summary card: label, big number, detail line.
fn stat_card(ui: &mut egui::Ui, palette: &theme::Palette, label: &str, value: String, detail: &str) {
    theme::card(ui, palette, |ui| {
        ui.set_width(ui.available_width());
        ui.label(
            egui::RichText::new(label)
                .size(11.0)
                .strong()
                .color(palette.text_dim),
        );
        ui.label(egui::RichText::new(value).size(28.0).strong());
        ui.label(
            egui::RichText::new(detail)
                .size(12.0)
                .color(palette.text_dim),
        );
    });
}

fn section_heading(ui: &mut egui::Ui, palette: &theme::Palette, title: &str, description: &str) {
    ui.add_space(18.0);
    ui.label(egui::RichText::new(title).size(17.0).strong());
    ui.label(
        egui::RichText::new(description)
            .size(13.0)
            .color(palette.text_dim),
    );
    ui.add_space(4.0);
}

impl DesktopApp {
    pub(super) fn show_systems(&mut self, ui: &mut egui::Ui) {
        let palette = theme::Palette::new(self.dark_mode, self.accent_color);
        let french = self.language == "fr";
        let system_ids: Vec<_> = self.registry.system_ids().collect();
        let system_count = system_ids.len();

        // ---------------------------------------------------------------- Header
        ui.horizontal(|ui| {
            ui.vertical(|ui| {
                ui.label(
                    egui::RichText::new(if french {
                        "Systèmes et modules"
                    } else {
                        "Systems and modules"
                    })
                    .size(30.0)
                    .strong(),
                );
                ui.label(
                    egui::RichText::new(if french {
                        "Disponibilité issue des cœurs enregistrés ; sélection conservée dans votre profil."
                    } else {
                        "Availability comes from registered cores; selection is persisted in your profile."
                    })
                    .color(palette.text_dim),
                );
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Min), |ui| {
                let noun = if french {
                    if system_count == 1 {
                        "système"
                    } else {
                        "systèmes"
                    }
                } else if system_count == 1 {
                    "system"
                } else {
                    "systems"
                };
                theme::badge(ui, &format!("{system_count} {noun}"), palette.accent);
            });
        });
        ui.add_space(6.0);
        ui.separator();
        ui.add_space(6.0);

        // ----------------------------------------------------------- Module state
        let state_path = match default_user_state_path() {
            Ok(path) => path,
            Err(error) => {
                theme::card(ui, &palette, |ui| {
                    ui.label(
                        egui::RichText::new(if french {
                            format!("État des modules indisponible : {error}")
                        } else {
                            format!("Module state unavailable: {error}")
                        })
                        .color(palette.danger),
                    );
                });
                return;
            }
        };
        let store = ModuleStateStore::new(state_path);
        let mut state = match store.load() {
            Ok(state) => state,
            Err(error) => {
                theme::card(ui, &palette, |ui| {
                    ui.label(
                        egui::RichText::new(if french {
                            format!("Impossible de lire l’état des modules : {error}")
                        } else {
                            format!("Could not load module state: {error}")
                        })
                        .strong()
                        .color(palette.danger),
                    );
                    ui.label(if french {
                        "Le fichier d’état est conservé pour permettre sa récupération ; aucune sélection n’a été écrasée."
                    } else {
                        "The state file was preserved for recovery; no selection was overwritten."
                    });
                });
                return;
            }
        };

        let mut selected = state.selected_modules().to_vec();
        let mut changed = false;

        // --------------------------------------------------------- Summary cards
        let stats = [
            (
                if french { "CŒURS" } else { "CORES" },
                system_count.to_string(),
                if french {
                    "enregistrés dans cette compilation"
                } else {
                    "registered in this build"
                },
            ),
            (
                if french { "ACTIVÉS" } else { "ENABLED" },
                selected.len().to_string(),
                if french {
                    "modules sélectionnés"
                } else {
                    "selected modules"
                },
            ),
            (
                if french { "PAQUETS" } else { "PACKAGES" },
                state.installed_modules().len().to_string(),
                if french {
                    "paquets installés"
                } else {
                    "installed packages"
                },
            ),
        ];
        let columns = if ui.available_width() < 620.0 { 1 } else { 3 };
        ui.columns(columns, |cols| {
            for (index, (label, value, detail)) in stats.into_iter().enumerate() {
                stat_card(&mut cols[index % columns], &palette, label, value, detail);
            }
        });

        // ------------------------------------------------------ Available modules
        section_heading(
            ui,
            &palette,
            if french {
                "Modules disponibles"
            } else {
                "Available modules"
            },
            if french {
                "Activez uniquement les modules que vous souhaitez utiliser. Cette sélection enregistre votre préférence : elle ne télécharge ni n’installe de paquet."
            } else {
                "Enable only the modules you want to use. This records your preference only: it does not download or install a package."
            },
        );

        if system_ids.is_empty() {
            theme::card(ui, &palette, |ui| {
                ui.set_width(ui.available_width());
                ui.label(
                    egui::RichText::new(if french {
                        "Aucun cœur n’est enregistré dans cette compilation."
                    } else {
                        "No emulation core is registered in this build."
                    })
                    .color(palette.text_dim),
                );
            });
        }

        for id in &system_ids {
            let Ok(module_id) = ModuleId::new(id.as_str().to_owned()) else {
                ui.label(
                    egui::RichText::new(format!(
                        "{}: {}",
                        if french {
                            "Identifiant non valide"
                        } else {
                            "Invalid module ID"
                        },
                        id.as_str()
                    ))
                    .color(palette.danger),
                );
                continue;
            };
            let available = self.registry.registered_descriptor(*id);
            let is_available = available.is_some();
            let installed_version = state.installed_version(&module_id).map(str::to_owned);
            let mut is_selected = selected.contains(&module_id);
            theme::card(ui, &palette, |ui| {
                ui.set_width(ui.available_width());
                ui.horizontal(|ui| {
                    module_tile(ui, &palette, id.as_str());
                    ui.vertical(|ui| {
                        ui.label(egui::RichText::new(id.as_str()).size(16.0).strong());
                        if let Some(version) = installed_version.as_deref() {
                            theme::badge(
                                ui,
                                &if french {
                                    format!("Installé · version {version}")
                                } else {
                                    format!("Installed · version {version}")
                                },
                                palette.success,
                            );
                        } else if is_available {
                            theme::badge(
                                ui,
                                if french {
                                    "Intégré à cette compilation"
                                } else {
                                    "Built into this build"
                                },
                                palette.accent,
                            );
                        } else {
                            theme::badge(
                                ui,
                                if french {
                                    "Non disponible"
                                } else {
                                    "Unavailable"
                                },
                                palette.warning,
                            );
                        }
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if toggle_switch(ui, &palette, &mut is_selected) {
                            changed = true;
                            if is_selected {
                                if !selected.contains(&module_id) {
                                    selected.push(module_id.clone());
                                }
                            } else {
                                selected.retain(|selected_id| selected_id != &module_id);
                            }
                        }
                        ui.label(
                            egui::RichText::new(if is_selected {
                                if french {
                                    "Activé"
                                } else {
                                    "Enabled"
                                }
                            } else if french {
                                "Désactivé"
                            } else {
                                "Disabled"
                            })
                            .size(13.0)
                            .color(palette.text_dim),
                        );
                    });
                });
                if let Some(descriptor) = available {
                    let caps = descriptor.capabilities;
                    ui.add_space(4.0);
                    ui.separator();
                    ui.label(
                        egui::RichText::new(if french { "CAPACITÉS" } else { "CAPABILITIES" })
                            .size(11.0)
                            .strong()
                            .color(palette.text_dim),
                    );
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
                            // The marker keeps the state readable without colour.
                            if enabled {
                                theme::badge(ui, &format!("● {label}"), palette.success);
                            } else {
                                theme::badge(ui, &format!("○ {label}"), palette.text_dim);
                            }
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
            section_heading(
                ui,
                &palette,
                if french {
                    "Sélections conservées"
                } else {
                    "Retained selections"
                },
                if french {
                    "Sélections conservées mais indisponibles dans cette compilation."
                } else {
                    "Selections retained but unavailable in this build."
                },
            );
            let mut deselected_unavailable = Vec::new();
            for selected_id in unavailable_selected {
                let mut keep_selected = true;
                theme::card(ui, &palette, |ui| {
                    ui.set_width(ui.available_width());
                    ui.horizontal(|ui| {
                        module_tile(ui, &palette, selected_id.as_str());
                        ui.vertical(|ui| {
                            ui.label(
                                egui::RichText::new(selected_id.as_str())
                                    .size(16.0)
                                    .strong(),
                            );
                            theme::badge(
                                ui,
                                if french {
                                    "Cœur absent"
                                } else {
                                    "Core not bundled"
                                },
                                palette.warning,
                            );
                        });
                        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                            if toggle_switch(ui, &palette, &mut keep_selected) {
                                deselected_unavailable.push(selected_id.clone());
                            }
                        });
                    });
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

        // ------------------------------------------------------ Installed modules
        section_heading(
            ui,
            &palette,
            if french {
                "Modules installés"
            } else {
                "Installed modules"
            },
            if french {
                "Les cœurs compilés et les paquets installés sont suivis séparément."
            } else {
                "Built-in cores and installed packages are tracked separately."
            },
        );
        theme::card(ui, &palette, |ui| {
            ui.set_width(ui.available_width());
            if state.installed_modules().is_empty() {
                ui.label(
                    egui::RichText::new(if french {
                        "Aucun paquet de module externe n’est enregistré comme installé."
                    } else {
                        "No external module package is recorded as installed."
                    })
                    .color(palette.text_dim),
                );
            } else {
                for installed in state.installed_modules() {
                    ui.horizontal_wrapped(|ui| {
                        ui.label(egui::RichText::new(installed.id.as_str()).strong());
                        theme::badge(ui, &format!("v{}", installed.version), palette.text_dim);
                        let bundled = system_ids
                            .iter()
                            .any(|system_id| system_id.as_str() == installed.id.as_str());
                        ui.label(
                            egui::RichText::new(if bundled {
                                if french {
                                    "Cœur également intégré"
                                } else {
                                    "Core also bundled"
                                }
                            } else if french {
                                "Paquet externe"
                            } else {
                                "External package"
                            })
                            .size(13.0)
                            .color(palette.text_dim),
                        );
                    });
                }
            }
        });
        ui.add_space(16.0);
    }
}
