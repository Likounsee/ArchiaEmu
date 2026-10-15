//! library page for the ArchiaEmu desktop frontend.

use super::*;
use archiaemu_app::library::{ContentAvailability, ContentFormatDetection, ContentValidation};

fn content_path_status(path: &Path) -> &'static str {
    match std::fs::metadata(path) {
        Ok(metadata) if metadata.is_file() => "File exists · launch validation pending",
        Ok(_) => "Unavailable · not a regular file",
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
            "Missing · re-import required"
        }
        Err(_) => "Unavailable · cannot read file metadata",
    }
}

impl DesktopApp {
    pub(super) fn show_library(&mut self, ui: &mut egui::Ui) {
        let total = self.controller.library().len();
        ui.horizontal_wrapped(|ui| {
            ui.vertical(|ui| {
                ui.heading(if self.language == "fr" { "Vos jeux" } else { "Your games" });
                ui.label(format!("{total} imported item(s)"));
            });
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button(if self.language == "fr" { "＋ Importer des ROM" } else { "＋ Import ROMs…" }).clicked() {
                    self.choose_content_file();
                }
            });
        });

        ui.add_space(8.0);
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.label("⌕ Search");
                ui.add(
                    egui::TextEdit::singleline(&mut self.library_filter)
                        .hint_text("Search titles and file paths…")
                        .desired_width(ui.available_width().min(260.0)),
                );
                egui::ComboBox::from_id_source("library_format_filter")
                    .selected_text(self.library_format.label())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::All,
                            "All formats",
                        );
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::GameBoy,
                            "Game Boy (.gb)",
                        );
                        ui.selectable_value(
                            &mut self.library_format,
                            LibraryFormat::GameBoyColor,
                            "Game Boy Color (.gbc)",
                        );
                    });
                egui::ComboBox::from_id_source("library_sort")
                    .selected_text(self.library_sort.label())
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.library_sort,
                            LibrarySort::Name,
                            "Title (A–Z)",
                        );
                        ui.selectable_value(
                            &mut self.library_sort,
                            LibrarySort::Largest,
                            "Largest first",
                        );
                        ui.selectable_value(&mut self.library_sort, LibrarySort::Path, "File path");
                    });
                if ui.button(if self.language == "fr" { "Effacer" } else { "Clear" }).clicked() {
                    self.library_filter.clear();
                    self.library_format = LibraryFormat::All;
                }
            });
        });

        let query = self.library_filter.trim().to_lowercase();
        let mut entries: Vec<_> = self
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
            .filter(|(title, path, _)| {
                (query.is_empty()
                    || title.to_lowercase().contains(&query)
                    || path.to_string_lossy().to_lowercase().contains(&query))
                    && self.library_format.matches(path)
            })
            .collect();
        match self.library_sort {
            LibrarySort::Name => entries.sort_by(|a, b| {
                a.0.to_lowercase()
                    .cmp(&b.0.to_lowercase())
                    .then_with(|| a.1.cmp(&b.1))
            }),
            LibrarySort::Largest => entries.sort_by(|a, b| {
                b.2.cmp(&a.2)
                    .then_with(|| a.0.to_lowercase().cmp(&b.0.to_lowercase()))
            }),
            LibrarySort::Path => entries.sort_by(|a, b| a.1.cmp(&b.1)),
        }

        ui.add_space(6.0);
        ui.horizontal(|ui| {
            ui.label(format!("Showing {} of {total}", entries.len()));
            ui.separator();
            let total_size: u64 = self
                .controller
                .library()
                .entries()
                .map(|entry| entry.size_bytes())
                .sum();
            ui.label(format!("{} total size", format_size(total_size)));
        });
        ui.add_space(6.0);

        if entries.is_empty() {
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.add_space(8.0);
                ui.heading(if total == 0 {
                    "Your library is empty"
                } else if query.is_empty() && self.library_format != LibraryFormat::All {
                    "No games in this format"
                } else {
                    "No matching games"
                });
                ui.label(if total == 0 {
                    "Import one or more .gb or .gbc files to build your collection."
                } else {
                    "Try another search, change the format filter, or clear the search."
                });
                if total == 0 && ui.button(if self.language == "fr" { "Parcourir les ROM" } else { "Browse for ROMs…" }).clicked() {
                    self.choose_content_file();
                }
            });
        } else {
            egui::ScrollArea::vertical()
                .auto_shrink([false, false])
                .show(ui, |ui| {
                    for (title, path, size) in entries {
                        let selected = self.selected_content.as_ref() == Some(&path);
                        egui::Frame::group(ui.style()).show(ui, |ui| {
                            ui.horizontal(|ui| {
                                ui.vertical(|ui| {
                                    ui.label(if selected {
                                        "▶ SELECTED"
                                    } else {
                                        "GAME BOY ROM"
                                    });
                                    ui.heading(&title);
                                    let extension = path
                                        .extension()
                                        .and_then(|value| value.to_str())
                                        .unwrap_or("unknown")
                                        .to_ascii_uppercase();
                                    ui.small(format!(
                                        "{extension} · {} · {}",
                                        format_size(size),
                                        path.display()
                                    ));
                                    let entry_metadata = self
                                        .controller
                                        .library()
                                        .get(&path)
                                        .map(|entry| {
                                            (
                                                entry.format_hint().map(str::to_owned),
                                                entry.compatible_system_hint().map(str::to_owned),
                                                entry.format_detection().clone(),
                                                entry.validation().clone(),
                                                entry.availability(),
                                            )
                                        });
                                    if let Some((
                                        format_hint,
                                        system_hint,
                                        detection_state,
                                        validation_state,
                                        availability_state,
                                    )) = entry_metadata
                                    {
                                        ui.small(format!(
                                            "Extension hint: {} · compatibility hint: {}",
                                            format_hint.as_deref().unwrap_or("unknown extension"),
                                            system_hint.as_deref().unwrap_or("undetermined")
                                        ));
                                        let detected = match &detection_state {
                                            ContentFormatDetection::Pending => {
                                                "Detected format: not probed yet"
                                            }
                                            ContentFormatDetection::NoMatch => {
                                                "Detected format: no registered probe matched"
                                            }
                                            ContentFormatDetection::Detected(format) => {
                                                ui.label(format!("Detected format: {format}"));
                                                ""
                                            }
                                            ContentFormatDetection::Ambiguous(formats) => {
                                                ui.colored_label(
                                                    egui::Color32::from_rgb(210, 115, 45),
                                                    format!(
                                                        "Ambiguous format probes: {}",
                                                        formats.join(", ")
                                                    ),
                                                );
                                                ""
                                            }
                                            ContentFormatDetection::NeedsMoreData => {
                                                "Detected format: probe needs more data"
                                            }
                                        };
                                        if !detected.is_empty() {
                                            ui.small(detected);
                                        }
                                        let validation = match &validation_state {
                                            ContentValidation::Pending => {
                                                "Validation: pending until launch"
                                            }
                                            ContentValidation::Accepted => {
                                                "Validation: last launch accepted"
                                            }
                                            ContentValidation::Rejected(_) => {
                                                "Validation: last launch rejected"
                                            }
                                        };
                                        ui.small(validation);
                                        if let ContentValidation::Rejected(detail) =
                                            &validation_state
                                        {
                                            ui.colored_label(egui::Color32::LIGHT_RED, detail);
                                        }
                                        let availability = match availability_state {
                                            ContentAvailability::Available => {
                                                "Available · ready for launch validation"
                                            }
                                            ContentAvailability::Missing => {
                                                "Missing · re-import required"
                                            }
                                            ContentAvailability::NotRegularFile => {
                                                "Unavailable · not a regular file"
                                            }
                                            ContentAvailability::Unavailable => {
                                                "Unavailable · cannot inspect file"
                                            }
                                        };
                                        ui.colored_label(
                                            if availability_state == ContentAvailability::Available
                                            {
                                                ui.visuals().weak_text_color()
                                            } else {
                                                egui::Color32::from_rgb(210, 115, 45)
                                            },
                                            availability,
                                        );
                                    } else {
                                        let path_status = content_path_status(&path);
                                        ui.colored_label(
                                            egui::Color32::from_rgb(210, 115, 45),
                                            path_status,
                                        );
                                    }
                                });
                                ui.with_layout(
                                    egui::Layout::right_to_left(egui::Align::Center),
                                    |ui| {
                                        if ui.button(if self.language == "fr" { "Retirer" } else { "Forget" }).clicked() {
                                            match self.controller.forget_content(&path) {
                                                Ok(_) => {
                                                    if self.selected_content.as_ref() == Some(&path)
                                                    {
                                                        self.selected_content = None;
                                                    }
                                                    let removal_message = format!(
                                                        "Removed '{}' from the library.",
                                                        title
                                                    );
                                                    self.message = removal_message.clone();
                                                    if !self.save_preferences() {
                                                        let save_error = std::mem::take(&mut self.message);
                                                        self.message = format!(
                                                            "{removal_message} The item is removed for this session, but the library change was not saved. {save_error}"
                                                        );
                                                    }
                                                }
                                                Err(error) => {
                                                    self.message =
                                                        format!("Could not forget content: {error}")
                                                }
                                            }
                                        }
                                        if ui
                                            .add_enabled(
                                                path.is_file(),
                                                egui::Button::new(if self.language == "fr" { "▶ Lancer" } else { "▶ Launch" }),
                                            )
                                            .clicked()
                                        {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                            self.launch_content();
                                        }
                                        if ui.button(if self.language == "fr" { "Utiliser ce chemin" } else { "Use path" }).clicked() {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                            self.message =
                                                format!("Selected '{}' for launch.", title);
                                        }
                                        if ui
                                            .button(if selected { "Selected" } else { "Select" })
                                            .clicked()
                                        {
                                            self.selected_content = Some(path.clone());
                                            self.content_path = path.display().to_string();
                                        }
                                    },
                                );
                            });
                        });
                    }
                });
        }

        let selected_details = self.selected_content.clone().and_then(|path| {
            self.controller
                .library()
                .entries()
                .find(|entry| entry.path() == path)
                .map(|entry| (path, entry.title().to_owned(), entry.size_bytes()))
        });
        if let Some((path, title, size)) = selected_details {
            ui.add_space(8.0);
            egui::Frame::group(ui.style()).show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.vertical(|ui| {
                        ui.heading(if self.language == "fr" { "Contenu sélectionné" } else { "Selected content" });
                        ui.label(&title);
                        ui.small(path.display().to_string());
                        ui.small(format!(
                            "{} · {}",
                            format_size(size),
                            content_path_status(&path)
                        ));
                    });
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui
                            .add_enabled(path.is_file(), egui::Button::new(if self.language == "fr" { "Lancer la sélection" } else { "Launch selected" }))
                            .clicked()
                        {
                            self.content_path = path.display().to_string();
                            self.launch_content();
                        }
                    });
                });
            });
        }
    }
}

#[cfg(test)]
mod tests {
    use super::content_path_status;
    use std::path::PathBuf;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn unique_temp_path(label: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system clock should be after the Unix epoch")
            .as_nanos();
        std::env::temp_dir().join(format!(
            "archiaemu-library-{label}-{}-{nonce}",
            std::process::id()
        ))
    }

    #[test]
    fn missing_library_path_requests_reimport() {
        let path = unique_temp_path("missing");
        assert_eq!(content_path_status(&path), "Missing · re-import required");
    }

    #[test]
    fn directory_is_not_reported_as_launchable_content() {
        let path = unique_temp_path("directory");
        std::fs::create_dir(&path).expect("create temporary test directory");

        assert_eq!(
            content_path_status(&path),
            "Unavailable · not a regular file"
        );

        std::fs::remove_dir(&path).expect("remove temporary test directory");
    }

    #[test]
    fn existing_regular_file_requires_launch_validation() {
        let path = unique_temp_path("file");
        std::fs::write(&path, b"test").expect("create temporary test file");

        assert_eq!(
            content_path_status(&path),
            "File exists · launch validation pending"
        );

        std::fs::remove_file(&path).expect("remove temporary test file");
    }
}
