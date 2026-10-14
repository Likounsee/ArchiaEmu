#![forbid(unsafe_code)]

//! Desktop preference persistence, isolated from UI composition and emulation.

use std::fs;
use std::path::{Path, PathBuf};

pub(crate) struct SavedPreferences {
    pub(crate) dark_mode: bool,
    pub(crate) ui_scale: f32,
    pub(crate) accent_rgb: [u8; 3],
    pub(crate) paths: Vec<PathBuf>,
}

pub(crate) fn config_file_path() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    let base = std::env::var_os("APPDATA").map(PathBuf::from);
    #[cfg(target_os = "macos")]
    let base = std::env::var_os("HOME")
        .map(|home| PathBuf::from(home).join("Library/Application Support"));
    #[cfg(all(unix, not(target_os = "macos")))]
    let base = std::env::var_os("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".config")));
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
            dark_mode = value == "true";
        } else if let Some(value) = line.strip_prefix("scale=") {
            if let Ok(parsed) = value.parse::<f32>() {
                if parsed.is_finite() {
                    ui_scale = parsed.clamp(0.8, 1.4);
                }
            }
        } else if let Some(value) = line.strip_prefix("accent=") {
            if value.len() == 6 && value.is_ascii() {
                if let (Ok(r), Ok(g), Ok(b)) = (u8::from_str_radix(&value[0..2], 16), u8::from_str_radix(&value[2..4], 16), u8::from_str_radix(&value[4..6], 16)) {
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
    let mut contents = format!("version=2\ndark={dark_mode}\nscale={ui_scale:.3}\naccent={:02X}{:02X}{:02X}\n", accent_rgb[0], accent_rgb[1], accent_rgb[2]);
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
    use super::{load_preferences, write_preferences};
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
}
