#![forbid(unsafe_code)]

//! Desktop preference persistence, isolated from UI composition and emulation.

use std::fs::{self, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

// Preferences are user-editable input. Bound reads so a damaged config cannot
// allocate unbounded memory during application startup.
const MAX_PREFERENCES_BYTES: usize = 4 * 1024 * 1024;

#[derive(Debug)]
pub(crate) struct SavedPreferences {
    pub(crate) dark_mode: bool,
    pub(crate) ui_scale: f32,
    pub(crate) accent_rgb: [u8; 3],
    pub(crate) paths: Vec<PathBuf>,
    pub(crate) language: String,
}

pub(crate) fn config_file_path() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    let base = std::env::var_os("APPDATA")
        .filter(|value| !value.is_empty())
        .or_else(|| std::env::var_os("LOCALAPPDATA").filter(|value| !value.is_empty()))
        .map(PathBuf::from);
    #[cfg(target_os = "macos")]
    let base = std::env::var_os("HOME")
        .filter(|value| !value.is_empty())
        .map(|home| PathBuf::from(home).join("Library/Application Support"));
    #[cfg(all(unix, not(target_os = "macos")))]
    let base = std::env::var_os("XDG_CONFIG_HOME")
        .filter(|value| !value.is_empty())
        .map(PathBuf::from)
        .or_else(|| {
            std::env::var_os("HOME")
                .filter(|value| !value.is_empty())
                .map(|home| PathBuf::from(home).join(".config"))
        });
    base.map(|directory| directory.join("ArchiaEmu").join("desktop.conf"))
}

fn read_preferences_file(path: &Path) -> std::io::Result<String> {
    let file = fs::File::open(path)?;
    let mut bytes = Vec::new();
    file.take(MAX_PREFERENCES_BYTES as u64 + 1)
        .read_to_end(&mut bytes)?;
    if bytes.len() > MAX_PREFERENCES_BYTES {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            format!("preferences file exceeds the {MAX_PREFERENCES_BYTES}-byte limit"),
        ));
    }
    String::from_utf8(bytes)
        .map_err(|error| std::io::Error::new(std::io::ErrorKind::InvalidData, error))
}

pub(crate) fn system_language() -> String {
    let locale = sys_locale::get_locale().unwrap_or_else(|| {
        ["LC_ALL", "LC_MESSAGES", "LANG"]
            .iter()
            .filter_map(|key| std::env::var(key).ok())
            .find(|value| !value.is_empty())
            .unwrap_or_default()
    });
    if locale.to_ascii_lowercase().starts_with("fr") {
        "fr".to_owned()
    } else {
        "en".to_owned()
    }
}

pub(crate) fn load_preferences(path: &Path) -> std::io::Result<Option<SavedPreferences>> {
    let contents = match read_preferences_file(path) {
        Ok(contents) => contents,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
            // A crash can occur after the previous config is moved aside but
            // before the freshly written temporary file is promoted. Recover
            // the last known-good backup instead of silently starting with defaults.
            let backup = path.with_extension("conf.bak");
            match read_preferences_file(&backup) {
                Ok(contents) => {
                    // Best effort: keep the recovered values usable even if restoring
                    // the filename fails due to a read-only or unusual filesystem.
                    let _ = fs::rename(&backup, path);
                    contents
                }
                Err(backup_error) if backup_error.kind() == std::io::ErrorKind::NotFound => {
                    return Ok(None);
                }
                Err(backup_error) => return Err(backup_error),
            }
        }
        Err(error) => return Err(error),
    };
    let mut dark_mode = true;
    let mut ui_scale = 1.0f32;
    let mut accent_rgb = [75, 115, 210];
    let mut paths = Vec::new();
    let mut language = system_language();
    for line in contents.lines() {
        if let Some(value) = line.strip_prefix("version=") {
            if value != "1" && value != "2" && value != "3" {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    format!("unsupported preferences version: {value}"),
                ));
            }
        } else if let Some(value) = line.strip_prefix("dark=") {
            match value {
                "true" => dark_mode = true,
                "false" => dark_mode = false,
                _ => {}
            }
        } else if let Some(value) = line.strip_prefix("scale=") {
            if let Ok(parsed) = value.parse::<f32>() {
                if parsed.is_finite() {
                    ui_scale = parsed.clamp(0.8, 1.4);
                }
            }
        } else if let Some(value) = line.strip_prefix("accent=") {
            if value.len() == 6 && value.is_ascii() {
                if let (Ok(r), Ok(g), Ok(b)) = (
                    u8::from_str_radix(&value[0..2], 16),
                    u8::from_str_radix(&value[2..4], 16),
                    u8::from_str_radix(&value[4..6], 16),
                ) {
                    accent_rgb = [r, g, b];
                }
            }
        } else if let Some(value) = line.strip_prefix("language=") {
            language = if value.eq_ignore_ascii_case("fr") {
                "fr"
            } else {
                "en"
            }
            .to_owned();
        } else if let Some(value) = line.strip_prefix("path=") {
            if let Some(decoded) = unescape_config_value(value) {
                paths.push(PathBuf::from(decoded));
            }
        }
    }
    Ok(Some(SavedPreferences {
        dark_mode,
        ui_scale,
        accent_rgb,
        paths,
        language,
    }))
}

pub(crate) fn write_preferences(
    path: &Path,
    dark_mode: bool,
    ui_scale: f32,
    accent_rgb: [u8; 3],
    paths: &[PathBuf],
) -> std::io::Result<()> {
    write_preferences_with_language(path, dark_mode, ui_scale, accent_rgb, "en", paths)
}

pub(crate) fn write_preferences_with_language(
    path: &Path,
    dark_mode: bool,
    ui_scale: f32,
    accent_rgb: [u8; 3],
    language: &str,
    paths: &[PathBuf],
) -> std::io::Result<()> {
    let parent = path.parent().ok_or_else(|| {
        std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "configuration path has no parent directory",
        )
    })?;
    fs::create_dir_all(parent)?;
    // Keep the on-disk value inside the same range enforced by the loader.
    // A non-finite value must not silently reset the user's scale on next launch.
    let ui_scale = if ui_scale.is_finite() {
        ui_scale.clamp(0.8, 1.4)
    } else {
        1.0
    };
    let mut contents = format!(
        "version=3\ndark={dark_mode}\nscale={ui_scale:.3}\naccent={:02X}{:02X}{:02X}\nlanguage={}\n",
        accent_rgb[0], accent_rgb[1], accent_rgb[2], language
    );
    for path in paths {
        contents.push_str("path=");
        contents.push_str(&escape_config_value(&path.to_string_lossy()));
        contents.push('\n');
        if contents.len() > MAX_PREFERENCES_BYTES {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                format!("serialized preferences exceed the {MAX_PREFERENCES_BYTES}-byte limit"),
            ));
        }
    }
    // Use a unique, create-new temporary file. A fixed .tmp path could be
    // left behind after a crash or redirected through a pre-existing symlink;
    // never truncate a file we did not create in this save operation.
    let nonce = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(std::io::Error::other)?
        .as_nanos();
    let temporary = path.with_extension(format!("conf.tmp-{}-{nonce}", std::process::id()));
    let backup = path.with_extension("conf.bak");
    let write_result = (|| {
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary)?;
        file.write_all(contents.as_bytes())?;
        // Flush the new configuration to the filesystem before promoting it.
        file.sync_all()
    })();
    if let Err(error) = write_result {
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }

    // Windows cannot rename over an existing destination. Move the current file
    // aside first, but restore it if promoting the new file fails.
    let had_previous = path.exists();
    if had_previous {
        if backup.exists() {
            if let Err(error) = fs::remove_file(&backup) {
                let _ = fs::remove_file(&temporary);
                return Err(error);
            }
        }
        if let Err(error) = fs::rename(path, &backup) {
            let _ = fs::remove_file(&temporary);
            return Err(error);
        }
    }

    if let Err(error) = fs::rename(&temporary, path) {
        if had_previous {
            let _ = fs::rename(&backup, path);
        }
        let _ = fs::remove_file(&temporary);
        return Err(error);
    }

    if had_previous {
        // The new config is already in place; stale-backup cleanup must not
        // turn a successful save into a reported failure.
        let _ = fs::remove_file(backup);
    }
    Ok(())
}

pub(crate) fn escape_config_value(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for character in value.chars() {
        match character {
            '\\' => escaped.push_str("\\\\"),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            other => escaped.push(other),
        }
    }
    escaped
}

pub(crate) fn unescape_config_value(value: &str) -> Option<String> {
    let mut decoded = String::with_capacity(value.len());
    let mut characters = value.chars();
    while let Some(character) = characters.next() {
        if character != '\\' {
            decoded.push(character);
            continue;
        }
        match characters.next()? {
            '\\' => decoded.push('\\'),
            'n' => decoded.push('\n'),
            'r' => decoded.push('\r'),
            _ => return None,
        }
    }
    Some(decoded)
}

#[cfg(test)]
mod tests {
    use super::{
        load_preferences, unescape_config_value, write_preferences, MAX_PREFERENCES_BYTES,
    };
    use std::fs;
    use std::path::PathBuf;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temporary_config(name: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system clock should be after UNIX epoch")
            .as_nanos();
        std::env::temp_dir().join(format!(
            "archiaemu-preferences-{name}-{}-{nonce}.conf",
            std::process::id()
        ))
    }

    #[test]
    fn oversized_preferences_and_recovery_backup_are_rejected() {
        let path = temporary_config("oversized-config");
        let mut oversized = vec![b'x'; MAX_PREFERENCES_BYTES + 1];
        // The first byte beyond the limit is invalid UTF-8. The size limit
        // must take precedence over text decoding for oversized input.
        oversized[MAX_PREFERENCES_BYTES] = 0xFF;
        fs::write(&path, oversized).expect("oversized preferences fixture should be written");

        let error = load_preferences(&path).expect_err("oversized config must be rejected");
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        assert!(error.to_string().contains("exceeds"));

        fs::remove_file(&path).expect("oversized config should be removed");
        let backup = path.with_extension("conf.bak");
        fs::write(&backup, vec![b'x'; MAX_PREFERENCES_BYTES + 1])
            .expect("oversized backup fixture should be written");
        let error = load_preferences(&path).expect_err("oversized backup must be rejected");
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        assert!(error.to_string().contains("exceeds"));

        let _ = fs::remove_file(path);
        let _ = fs::remove_file(backup);
    }

    #[test]
    fn oversized_save_does_not_replace_existing_preferences() {
        let path = temporary_config("preserve-existing-on-oversized-save");
        let original = b"version=2\\ndark=false\\nscale=1.2\\n";
        fs::write(&path, original).expect("existing config should be written");
        let oversized_path = PathBuf::from("x".repeat(MAX_PREFERENCES_BYTES + 1));

        let error = write_preferences(&path, true, 1.0, [75, 115, 210], &[oversized_path])
            .expect_err("oversized replacement must be rejected");

        assert_eq!(error.kind(), std::io::ErrorKind::InvalidInput);
        assert_eq!(
            fs::read(&path).expect("existing config should remain"),
            original
        );
        let _ = fs::remove_file(path);
    }

    #[test]
    fn refuses_to_write_preferences_that_loader_would_reject() {
        let path = temporary_config("oversized-write");
        let oversized_path = PathBuf::from("x".repeat(MAX_PREFERENCES_BYTES + 1));

        let error = write_preferences(&path, true, 1.0, [75, 115, 210], &[oversized_path])
            .expect_err("oversized serialized preferences must be rejected");
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidInput);
        assert!(error.to_string().contains("serialized preferences exceed"));
        assert!(
            !path.exists(),
            "rejected preferences must not create a config file"
        );
    }

    #[test]
    fn saves_and_restores_accent_color() {
        let path = temporary_config("accent-roundtrip");
        let paths = vec![PathBuf::from("library/game.gb")];
        write_preferences(&path, false, 1.1, [12, 144, 231], &paths)
            .expect("preferences should be written");

        let saved = load_preferences(&path)
            .expect("preferences should load")
            .expect("saved preferences should exist");
        assert!(!saved.dark_mode);
        assert!((saved.ui_scale - 1.1).abs() < f32::EPSILON);
        assert_eq!(saved.accent_rgb, [12, 144, 231]);
        assert_eq!(saved.paths, paths);
        let _ = fs::remove_file(path);
    }

    #[test]
    fn saving_preferences_does_not_truncate_a_preexisting_temporary_path() {
        let path = temporary_config("preserve-stale-temp");
        let stale_temporary = path.with_extension("conf.tmp");
        fs::write(&stale_temporary, b"preserve unrelated existing file")
            .expect("stale temporary sentinel should be created");

        write_preferences(&path, true, 1.0, [75, 115, 210], &[])
            .expect("preferences should save through a unique temporary file");

        assert_eq!(
            fs::read(&stale_temporary).expect("pre-existing file should remain"),
            b"preserve unrelated existing file"
        );
        let _ = fs::remove_file(path);
        let _ = fs::remove_file(stale_temporary);
    }

    #[test]
    fn version_one_preferences_keep_default_accent() {
        let path = temporary_config("legacy-v1");
        fs::write(&path, "version=1\ndark=false\nscale=1.0\n")
            .expect("legacy preferences should be written");

        let saved = load_preferences(&path)
            .expect("legacy preferences should load")
            .expect("legacy preferences should exist");
        assert!(!saved.dark_mode);
        assert_eq!(saved.accent_rgb, [75, 115, 210]);
        let _ = fs::remove_file(path);
    }

    #[test]
    fn malformed_dark_mode_value_does_not_override_a_valid_setting() {
        let path = temporary_config("malformed-dark");
        fs::write(
            &path,
            "version=2\ndark=false\ndark=not-a-boolean\nscale=1.0\n",
        )
        .expect("preferences should be written");

        let saved = load_preferences(&path)
            .expect("malformed optional value should not invalidate the whole file")
            .expect("preferences should exist");
        assert!(
            !saved.dark_mode,
            "invalid value must preserve the last valid value"
        );
        let _ = fs::remove_file(path);
    }

    #[test]
    fn unknown_preferences_version_is_rejected() {
        let path = temporary_config("unknown-version");
        fs::write(&path, "version=99\ndark=false\n").expect("preferences should be written");

        let error = load_preferences(&path).expect_err("unknown version must be rejected");
        assert_eq!(error.kind(), std::io::ErrorKind::InvalidData);
        assert!(error
            .to_string()
            .contains("unsupported preferences version"));
        let _ = fs::remove_file(path);
    }

    #[test]
    fn malformed_values_fall_back_to_safe_defaults() {
        let path = temporary_config("malformed-values");
        fs::write(
            &path,
            "version=2\ndark=not-a-bool\nscale=NaN\naccent=GG11ZZ\npath=bad\\qpath\n",
        )
        .expect("malformed preferences should be written");

        let saved = load_preferences(&path)
            .expect("malformed values should not crash loading")
            .expect("preferences file should exist");
        assert!(saved.dark_mode);
        assert!((saved.ui_scale - 1.0).abs() < f32::EPSILON);
        assert_eq!(saved.accent_rgb, [75, 115, 210]);
        assert!(saved.paths.is_empty());
        let _ = fs::remove_file(path);
    }

    #[test]
    fn saving_preferences_normalizes_non_finite_and_out_of_range_scale() {
        let path = temporary_config("normalized-scale");
        for (input, expected) in [
            (f32::NAN, 1.0),
            (f32::INFINITY, 1.0),
            (0.1, 0.8),
            (2.0, 1.4),
        ] {
            write_preferences(&path, true, input, [75, 115, 210], &[])
                .expect("preferences should save with a normalized scale");
            let saved = load_preferences(&path)
                .expect("normalized preferences should load")
                .expect("saved preferences should exist");
            assert!(
                (saved.ui_scale - expected).abs() < f32::EPSILON,
                "input scale {input:?} should normalize to {expected}"
            );
        }
        let _ = fs::remove_file(path);
    }

    #[test]
    fn recovers_previous_preferences_when_crash_left_only_backup() {
        let path = temporary_config("crash-recovery");
        let backup = path.with_extension("conf.bak");
        fs::write(&backup, "version=2\ndark=false\nscale=1.2\n")
            .expect("backup preferences should be written");

        let saved = load_preferences(&path)
            .expect("backup preferences should recover")
            .expect("backup should provide preferences");
        assert!(!saved.dark_mode);
        assert!((saved.ui_scale - 1.2).abs() < f32::EPSILON);
        assert!(
            path.exists(),
            "recovery should restore the active config path"
        );
        assert!(
            !backup.exists(),
            "successful rename should consume the backup"
        );

        let _ = fs::remove_file(path);
        let _ = fs::remove_file(backup);
    }

    #[test]
    fn missing_preferences_file_is_a_normal_first_run() {
        let path = temporary_config("missing-file");
        let _ = fs::remove_file(&path);
        assert!(load_preferences(&path)
            .expect("missing preferences should not be an I/O failure")
            .is_none());
    }

    #[test]
    fn escaped_path_values_round_trip_without_creating_extra_settings() {
        let path = temporary_config("escaped-path-roundtrip");
        let paths = vec![
            PathBuf::from("library/line\nbreak\\\\game.gb"),
            PathBuf::from("library/carriage\rreturn.gb"),
            PathBuf::from("library/trailing-backslash\\\\"),
        ];

        write_preferences(&path, true, 1.0, [75, 115, 210], &paths)
            .expect("preferences with escaped paths should be written");
        let saved = load_preferences(&path)
            .expect("preferences with escaped paths should load")
            .expect("saved preferences should exist");

        assert_eq!(saved.paths, paths);
        assert!(saved.dark_mode);
        let _ = fs::remove_file(path);
    }

    #[test]
    fn stale_temporary_path_does_not_block_save_or_damage_previous_preferences() {
        let path = temporary_config("preserve-previous");
        let stale_temporary = path.with_extension("conf.tmp");
        fs::write(&path, "version=2\ndark=false\n")
            .expect("previous preferences should be written");
        fs::create_dir(&stale_temporary).expect("stale temporary path should be created");

        write_preferences(&path, true, 1.0, [75, 115, 210], &[])
            .expect("unique temporary file should allow save despite stale path");

        let saved = load_preferences(&path)
            .expect("new preferences should load")
            .expect("new preferences should exist");
        assert!(saved.dark_mode, "new complete preferences should be active");
        assert!(
            stale_temporary.is_dir(),
            "save must not remove or overwrite a stale path it did not create"
        );

        let _ = fs::remove_dir(stale_temporary);
        let _ = fs::remove_file(path);
    }

    #[test]
    fn saving_preferences_reports_unwritable_parent_directory() {
        let blocker = temporary_config("parent-is-file");
        fs::write(&blocker, "not a directory").expect("blocker file should be created");
        let path = blocker.join("desktop.conf");

        let error = write_preferences(&path, true, 1.0, [75, 115, 210], &[])
            .expect_err("a file cannot be used as the configuration parent");
        assert!(
            matches!(
                error.kind(),
                std::io::ErrorKind::NotADirectory | std::io::ErrorKind::AlreadyExists
            ),
            "unexpected error kind: {error}"
        );
        let _ = fs::remove_file(blocker);
    }

    #[test]
    fn malformed_escape_sequences_are_rejected_without_panicking() {
        assert_eq!(
            unescape_config_value("valid\\\\path"),
            Some("valid\\path".to_owned())
        );
        assert_eq!(
            unescape_config_value("line\\nnext"),
            Some("line\nnext".to_owned())
        );
        assert_eq!(unescape_config_value("bad\\qescape"), None);
        assert_eq!(unescape_config_value("trailing\\"), None);
    }

    #[test]
    fn out_of_range_ui_scale_is_clamped_to_supported_limits() {
        let path = temporary_config("scale-clamping");
        fs::write(&path, "version=2\nscale=9.0\n").expect("preferences should be written");

        let saved = load_preferences(&path)
            .expect("out-of-range scale should be handled")
            .expect("preferences should exist");
        assert!((saved.ui_scale - 1.4).abs() < f32::EPSILON);
        fs::write(&path, "version=2\nscale=0.1\n").expect("low scale should be written");
        let saved = load_preferences(&path)
            .expect("low scale should be handled")
            .expect("preferences should exist");
        assert!((saved.ui_scale - 0.8).abs() < f32::EPSILON);
        let _ = fs::remove_file(path);
    }
}
