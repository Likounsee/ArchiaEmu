#![forbid(unsafe_code)]

//! Desktop preference persistence, isolated from UI composition and emulation.

use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug)]
pub(crate) struct SavedPreferences {
    pub(crate) dark_mode: bool,
    pub(crate) ui_scale: f32,
    pub(crate) accent_rgb: [u8; 3],
    pub(crate) paths: Vec<PathBuf>,
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

pub(crate) fn load_preferences(path: &Path) -> std::io::Result<Option<SavedPreferences>> {
    let contents = match fs::read_to_string(path) {
        Ok(contents) => contents,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    let mut dark_mode = true;
    let mut ui_scale = 1.0f32;
    let mut accent_rgb = [75, 115, 210];
    let mut paths = Vec::new();
    for line in contents.lines() {
        if let Some(value) = line.strip_prefix("version=") {
            if value != "1" && value != "2" {
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
    }))
}

pub(crate) fn write_preferences(
    path: &Path,
    dark_mode: bool,
    ui_scale: f32,
    accent_rgb: [u8; 3],
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
        "version=2\ndark={dark_mode}\nscale={ui_scale:.3}\naccent={:02X}{:02X}{:02X}\n",
        accent_rgb[0], accent_rgb[1], accent_rgb[2]
    );
    for path in paths {
        contents.push_str("path=");
        contents.push_str(&escape_config_value(&path.to_string_lossy()));
        contents.push('\n');
    }
    let temporary = path.with_extension("conf.tmp");
    let backup = path.with_extension("conf.bak");
    fs::write(&temporary, contents)?;

    // Windows cannot rename over an existing destination. Move the current file
    // aside first, but restore it if promoting the new file fails.
    let had_previous = path.exists();
    if had_previous {
        if backup.exists() {
            fs::remove_file(&backup)?;
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
    use super::{load_preferences, unescape_config_value, write_preferences};
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
    fn failed_temporary_write_preserves_the_previous_preferences_file() {
        let path = temporary_config("preserve-previous");
        let temporary = path.with_extension("conf.tmp");
        fs::write(&path, "version=2\ndark=false\n")
            .expect("previous preferences should be written");
        fs::create_dir(&temporary)
            .expect("temporary path should block creation of a temporary file");

        let error = write_preferences(&path, true, 1.0, [75, 115, 210], &[])
            .expect_err("writing over a directory should fail");
        // Filesystem error kinds differ between Windows, macOS, and Linux.
        assert!(!error.to_string().is_empty());
        assert_eq!(
            fs::read_to_string(&path).expect("previous preferences must remain readable"),
            "version=2\ndark=false\n"
        );

        let _ = fs::remove_dir(temporary);
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
        let _ = fs::write(&path, "version=2\nscale=0.1\n").expect("low scale should be written");
        let saved = load_preferences(&path)
            .expect("low scale should be handled")
            .expect("preferences should exist");
        assert!((saved.ui_scale - 0.8).abs() < f32::EPSILON);
        let _ = fs::remove_file(path);
    }
}
