#![forbid(unsafe_code)]

//! Application update metadata and staged-artifact integrity checks.
//! This is deliberately separate from module downloads and activation.

use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::time::Duration;

pub(crate) const MANIFEST_URL: &str =
    "https://raw.githubusercontent.com/Likounsee/ArchiaEmu/ArchiaEmu/ArchiaEmu/update/stable.json";
const MAX_MANIFEST_BYTES: u64 = 1024 * 1024;
const MAX_DOWNLOAD_BYTES: u64 = 2 * 1024 * 1024 * 1024;

#[derive(Clone, Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct UpdateManifest {
    pub(crate) schema_version: u32,
    pub(crate) version: String,
    pub(crate) release_notes_url: String,
    #[serde(default)]
    pub(crate) downloads: BTreeMap<String, UpdateDownload>,
}

#[derive(Clone, Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct UpdateDownload {
    pub(crate) url: String,
    pub(crate) size_bytes: u64,
    pub(crate) sha256: String,
}

impl UpdateManifest {
    pub(crate) fn validate(&self) -> Result<(), String> {
        if self.schema_version != 1 {
            return Err(format!(
                "Unsupported update manifest schema: {}",
                self.schema_version
            ));
        }
        parse_version(&self.version)
            .ok_or_else(|| format!("Invalid application version: {}", self.version))?;
        validate_release_notes_url(&self.release_notes_url)?;
        for (platform, download) in &self.downloads {
            if !is_supported_platform_key(platform) {
                return Err(format!("Unsupported update platform key: {platform}."));
            }
            validate_download_url(&download.url)?;
            if download.size_bytes == 0 || download.size_bytes > MAX_DOWNLOAD_BYTES {
                return Err(format!("Invalid download size for platform {platform}."));
            }
            if download.sha256.len() != 64
                || !download.sha256.bytes().all(|byte| byte.is_ascii_hexdigit())
            {
                return Err(format!("Invalid SHA-256 digest for platform {platform}."));
            }
        }
        Ok(())
    }

    pub(crate) fn download_for_current_platform(&self) -> Option<&UpdateDownload> {
        self.downloads.get(current_platform_key())
    }
}

pub(crate) fn fetch_manifest() -> Result<UpdateManifest, String> {
    let agent = ureq::AgentBuilder::new()
        .timeout_connect(Duration::from_secs(5))
        .timeout_read(Duration::from_secs(5))
        .build();
    let response = agent
        .get(MANIFEST_URL)
        .call()
        .map_err(|error| format!("Could not reach the official update manifest: {error}"))?;
    let mut body = String::new();
    response
        .into_reader()
        .take(MAX_MANIFEST_BYTES + 1)
        .read_to_string(&mut body)
        .map_err(|error| format!("Could not read update manifest: {error}"))?;
    if body.len() as u64 > MAX_MANIFEST_BYTES {
        return Err("Update manifest exceeds the 1 MiB safety limit.".to_owned());
    }
    let manifest: UpdateManifest = serde_json::from_str(&body)
        .map_err(|error| format!("Official update manifest is invalid: {error}"))?;
    manifest.validate()?;
    Ok(manifest)
}

/// Downloads a published application artifact into a private staging directory,
/// verifies its declared length and SHA-256, and never executes or installs it.
pub(crate) fn download_and_verify(
    download: &UpdateDownload,
    version: &str,
) -> Result<PathBuf, String> {
    parse_version(version)
        .ok_or_else(|| format!("Invalid application version for update staging: {version}"))?;
    validate_download_url(&download.url)?;
    if download.size_bytes == 0 || download.size_bytes > MAX_DOWNLOAD_BYTES {
        return Err("Update download size is outside the accepted limits.".to_owned());
    }
    if download.sha256.len() != 64 || !download.sha256.bytes().all(|byte| byte.is_ascii_hexdigit())
    {
        return Err("Update download has a malformed SHA-256 digest.".to_owned());
    }

    let directory = std::env::temp_dir().join("ArchiaEmu").join("updates");
    fs::create_dir_all(&directory)
        .map_err(|error| format!("Could not create update staging directory: {error}"))?;
    let nonce = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(|error| format!("System clock is invalid: {error}"))?
        .as_nanos();
    let safe_version: String = version
        .chars()
        .map(|character| {
            if character.is_ascii_alphanumeric() || character == '.' || character == '-' {
                character
            } else {
                '_'
            }
        })
        .collect();
    let partial_path = directory.join(format!("archiaemu-{safe_version}-{nonce}.part"));
    let verified_path = directory.join(format!("archiaemu-{safe_version}-{nonce}.verified"));

    // Only clean up paths that this invocation successfully created. A collision on
    // either create_new() or rename() must never cause us to delete a pre-existing file.
    let mut partial_created = false;
    let mut verified_created = false;
    let result = (|| {
        let agent = ureq::AgentBuilder::new()
            .timeout_connect(Duration::from_secs(10))
            .timeout_read(Duration::from_secs(30))
            .build();
        let response = agent
            .get(&download.url)
            .call()
            .map_err(|error| format!("Could not download the update artifact: {error}"))?;
        // GitHub release URLs commonly redirect to release-assets.githubusercontent.com.
        // Validate the final destination too, rather than trusting only the manifest URL.
        validate_download_url(response.get_url())?;
        validate_content_length(response.header("Content-Length"), download.size_bytes)?;
        let mut reader = response.into_reader();
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&partial_path)
            .map_err(|error| format!("Could not create staged update file: {error}"))?;
        partial_created = true;
        let mut hasher = Sha256::new();
        let mut total = 0_u64;
        let mut buffer = [0_u8; 64 * 1024];
        loop {
            let count = reader
                .read(&mut buffer)
                .map_err(|error| format!("Update download was interrupted: {error}"))?;
            if count == 0 {
                break;
            }
            total = total.saturating_add(count as u64);
            if total > download.size_bytes || total > MAX_DOWNLOAD_BYTES {
                return Err("Update download exceeded its declared size limit.".to_owned());
            }
            file.write_all(&buffer[..count])
                .map_err(|error| format!("Could not write staged update: {error}"))?;
            hasher.update(&buffer[..count]);
        }
        file.sync_all()
            .map_err(|error| format!("Could not flush staged update: {error}"))?;
        if total != download.size_bytes {
            return Err(format!(
                "Update download size mismatch (expected {} bytes, received {total}).",
                download.size_bytes
            ));
        }
        let actual = format!("{:x}", hasher.finalize());
        if !actual.eq_ignore_ascii_case(&download.sha256) {
            return Err(
                "Downloaded update SHA-256 does not match the official manifest.".to_owned(),
            );
        }
        // A rename may replace an existing destination on some platforms. Creating
        // a hard link is atomic and fails if the verified path already exists.
        create_verified_link(&partial_path, &verified_path)?;
        verified_created = true;
        fs::remove_file(&partial_path)
            .map_err(|error| format!("Could not remove temporary update file after verification: {error}"))?;
        partial_created = false;
        Ok(verified_path.clone())
    })();

    if result.is_err() {
        if partial_created {
            let _ = fs::remove_file(&partial_path);
        }
        if verified_created {
            let _ = fs::remove_file(&verified_path);
        }
    }
    result
}

/// Reject a contradictory or malformed HTTP length before creating a staged file.
fn validate_content_length(header: Option<&str>, expected_size: u64) -> Result<(), String> {
    let Some(header) = header else {
        // Chunked or otherwise length-less responses are still bounded by the streaming check.
        return Ok(());
    };
    let declared_size = header
        .parse::<u64>()
        .map_err(|_| "Update server returned an invalid Content-Length header.".to_owned())?;
    if declared_size != expected_size {
        return Err(format!(
            "Update server Content-Length mismatch (expected {expected_size} bytes, declared {declared_size})."
        ));
    }
    Ok(())
}

fn create_verified_link(partial_path: &Path, verified_path: &Path) -> Result<(), String> {
    fs::hard_link(partial_path, verified_path).map_err(|error| {
        format!(
            "Could not finalize verified update file without overwriting an existing file: {error}"
        )
    })
}

fn validate_release_notes_url(url: &str) -> Result<(), String> {
    validate_https_url(url, "release notes")?;
    if url.contains('?') || url.contains('#') {
        return Err("Release notes URL must not contain a query or fragment.".to_owned());
    }
    let rest = url
        .strip_prefix("https://")
        .ok_or_else(|| "Release notes must use HTTPS.".to_owned())?;
    let (authority, path) = rest
        .split_once('/')
        .map(|(authority, path)| (authority, format!("/{path}")))
        .unwrap_or((rest, String::new()));
    if !authority.eq_ignore_ascii_case("github.com")
        || authority.contains('@')
        || authority.contains(':')
        || !(path == "/Likounsee/ArchiaEmu/releases"
            || path.starts_with("/Likounsee/ArchiaEmu/releases/"))
    {
        return Err(
            "Release notes must point to the official ArchiaEmu GitHub releases page.".to_owned(),
        );
    }
    Ok(())
}

fn validate_download_url(url: &str) -> Result<(), String> {
    validate_https_url(url, "download")?;
    // Do not normalize user-info or ports away before checking the host: doing so
    // can make a URL whose actual destination is untrusted appear to be GitHub-hosted.
    let authority = url
        .strip_prefix("https://")
        .and_then(|rest| rest.split('/').next())
        .unwrap_or_default();
    if authority.contains('@') || authority.contains(':') {
        return Err(
            "Update artifact URLs must use an official GitHub host without credentials or custom ports."
                .to_owned(),
        );
    }
    let authority = authority.to_ascii_lowercase();
    if !matches!(
        authority.as_str(),
        "github.com" | "objects.githubusercontent.com" | "release-assets.githubusercontent.com"
    ) {
        return Err(
            "Update artifacts must be hosted on official GitHub release domains.".to_owned(),
        );
    }
    // A github.com URL must identify this project's release asset, not merely
    // any page or release in any repository on the trusted domain. Redirected
    // release-assets.githubusercontent.com URLs are checked by the host allowlist.
    if authority == "github.com" {
        let path = url
            .strip_prefix("https://")
            .and_then(|rest| rest.split_once('/').map(|(_, path)| format!("/{path}")))
            .unwrap_or_default();
        if !path.starts_with("/Likounsee/ArchiaEmu/releases/download/") {
            return Err(
                "Update artifacts must come from an ArchiaEmu GitHub release.".to_owned(),
            );
        }
    }
    Ok(())
}

pub(crate) fn is_newer_version(candidate: &str, current: &str) -> bool {
    match (parse_version(candidate), parse_version(current)) {
        (Some(candidate), Some(current)) => candidate > current,
        _ => false,
    }
}

fn parse_version(version: &str) -> Option<(u64, u64, u64)> {
    let mut parts = version.split('.');
    let parse_component = |part: &str| {
        if part.is_empty()
            || !part.bytes().all(|byte| byte.is_ascii_digit())
            || (part.len() > 1 && part.starts_with('0'))
        {
            return None;
        }
        part.parse().ok()
    };
    let major = parse_component(parts.next()?)?;
    let minor = parse_component(parts.next()?)?;
    let patch = parse_component(parts.next()?)?;
    if parts.next().is_some() {
        return None;
    }
    Some((major, minor, patch))
}

fn validate_https_url(url: &str, label: &str) -> Result<(), String> {
    if url
        .chars()
        .any(|character| character.is_control() || character == char::from(92))
    {
        return Err(format!(
            "The {label} URL contains a forbidden control character or backslash."
        ));
    }
    let authority = url
        .strip_prefix("https://")
        .and_then(|rest| rest.split(['/', '?', '#']).next())
        .unwrap_or_default();
    if authority.is_empty()
        || authority.starts_with('.')
        || authority.ends_with('.')
        || authority.contains('@')
        || authority.chars().any(char::is_whitespace)
    {
        return Err(format!(
            "The {label} URL must use HTTPS and contain a valid host."
        ));
    }
    Ok(())
}

/// Verifies a staged update without executing it or changing installed files.
#[allow(dead_code)] // Used when the downloader/apply transaction is introduced.
pub(crate) fn verify_staged_file(
    path: &Path,
    expected_size: u64,
    expected_sha256: &str,
) -> Result<(), String> {
    if expected_size == 0 || expected_size > MAX_DOWNLOAD_BYTES {
        return Err("Staged update size is outside the accepted limits.".to_owned());
    }
    if expected_sha256.len() != 64 || !expected_sha256.bytes().all(|byte| byte.is_ascii_hexdigit())
    {
        return Err("Expected SHA-256 digest is malformed.".to_owned());
    }
    let metadata = std::fs::metadata(path)
        .map_err(|error| format!("Could not inspect staged update: {error}"))?;
    if !metadata.is_file() || metadata.len() != expected_size {
        return Err(format!(
            "Staged update size mismatch (expected {expected_size} bytes, found {}).",
            metadata.len()
        ));
    }
    let mut file =
        File::open(path).map_err(|error| format!("Could not open staged update: {error}"))?;
    let mut hasher = Sha256::new();
    let mut buffer = [0u8; 64 * 1024];
    loop {
        let count = file
            .read(&mut buffer)
            .map_err(|error| format!("Could not hash staged update: {error}"))?;
        if count == 0 {
            break;
        }
        hasher.update(&buffer[..count]);
    }
    let actual = format!("{:x}", hasher.finalize());
    if !actual.eq_ignore_ascii_case(expected_sha256) {
        return Err("Staged update SHA-256 does not match the trusted manifest.".to_owned());
    }
    Ok(())
}

#[cfg(test)]
pub(crate) fn write_test_file(path: &Path, contents: &[u8]) {
    let mut file = File::create(path).expect("test file should be created");
    file.write_all(contents)
        .expect("test file should be written");
}

fn is_supported_platform_key(platform: &str) -> bool {
    matches!(
        platform,
        "windows-x86_64" | "macos-aarch64" | "macos-x86_64" | "linux-x86_64"
    )
}

fn current_platform_key() -> &'static str {
    if cfg!(all(target_os = "windows", target_arch = "x86_64")) {
        "windows-x86_64"
    } else if cfg!(all(target_os = "macos", target_arch = "aarch64")) {
        "macos-aarch64"
    } else if cfg!(all(target_os = "macos", target_arch = "x86_64")) {
        "macos-x86_64"
    } else if cfg!(all(target_os = "linux", target_arch = "x86_64")) {
        "linux-x86_64"
    } else {
        "unsupported"
    }
}

#[cfg(test)]
mod tests {
    use super::{
        is_newer_version, validate_download_url, verify_staged_file, write_test_file,
        UpdateManifest,
    };
    use sha2::{Digest, Sha256};
    use std::path::PathBuf;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temporary_file() -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be valid")
            .as_nanos();
        std::env::temp_dir().join(format!(
            "archiaemu-update-{}-{nonce}.bin",
            std::process::id()
        ))
    }

    #[test]
    fn finalizing_verified_update_never_overwrites_an_existing_destination() {
        let source = temporary_file();
        let mut destination = temporary_file();
        destination.set_extension("existing");
        write_test_file(&source, b"verified update");
        write_test_file(&destination, b"keep this file");

        let result = super::create_verified_link(&source, &destination);

        assert!(result.is_err());
        assert_eq!(
            std::fs::read(&destination).expect("existing destination should remain"),
            b"keep this file"
        );
        assert_eq!(
            std::fs::read(&source).expect("source should remain after collision"),
            b"verified update"
        );
        let _ = std::fs::remove_file(source);
        let _ = std::fs::remove_file(destination);
    }

    #[test]
    fn finalizing_verified_update_preserves_contents_at_the_new_path() {
        let source = temporary_file();
        let mut destination = temporary_file();
        destination.set_extension("verified");
        write_test_file(&source, b"verified update");

        super::create_verified_link(&source, &destination)
            .expect("new verified destination should be created");

        assert_eq!(
            std::fs::read(&destination).expect("verified destination should be readable"),
            b"verified update"
        );
        assert_eq!(
            std::fs::read(&source).expect("source should remain until caller removes it"),
            b"verified update"
        );
        let _ = std::fs::remove_file(source);
        let _ = std::fs::remove_file(destination);
    }

    #[test]
    fn validates_a_manifest_without_publishing_unavailable_downloads() {
        let manifest: UpdateManifest = serde_json::from_str(
            r#"{"schema_version":1,"version":"0.1.0","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{}}"#,
        )
        .expect("manifest JSON should parse");
        manifest.validate().expect("empty release channel is valid");
        assert!(manifest.download_for_current_platform().is_none());
    }

    #[test]
    fn accepts_each_supported_update_platform_key() {
        for platform in [
            "windows-x86_64",
            "macos-aarch64",
            "macos-x86_64",
            "linux-x86_64",
        ] {
            let manifest: UpdateManifest = serde_json::from_value(serde_json::json!({
                "schema_version": 1,
                "version": "0.2.0",
                "release_notes_url": "https://github.com/Likounsee/ArchiaEmu/releases",
                "downloads": {
                    (platform): {
                        "url": "https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app",
                        "size_bytes": 1,
                        "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                    }
                }
            }))
            .expect("manifest JSON should parse");
            manifest.validate().expect("supported platform key should be accepted");
        }
    }

    #[test]
    fn rejects_unknown_update_platform_keys_instead_of_silently_ignoring_them() {
        let manifest: UpdateManifest = serde_json::from_str(
            r#"{"schema_version":1,"version":"0.2.0","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{"linux-x64":{"url":"https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app","size_bytes":1,"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}}"#,
        )
        .expect("manifest JSON should parse");
        assert!(manifest.validate().is_err());
    }

    #[test]
    fn rejects_unknown_manifest_and_download_fields() {
        let unknown_manifest_field = serde_json::from_str::<UpdateManifest>(
            r#"{"schema_version":1,"version":"0.2.0","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{},"unexpected":true}"#,
        );
        assert!(unknown_manifest_field.is_err());

        let unknown_download_field = serde_json::from_str::<UpdateManifest>(
            r#"{"schema_version":1,"version":"0.2.0","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{"linux-x86_64":{"url":"https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app","size_bytes":1,"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","signature":"ignored-by-old-clients"}}}"#,
        );
        assert!(unknown_download_field.is_err());
    }

    #[test]
    fn rejects_release_notes_outside_the_official_project_releases() {
        for url in [
            "https://example.com/releases",
            "https://github.com/another/project/releases",
            "https://github.com/Likounsee/ArchiaEmu/releases-evil",
            "https://attacker@github.com/Likounsee/ArchiaEmu/releases",
            "https://github.com:8443/Likounsee/ArchiaEmu/releases",
            "https://github.com/Likounsee/ArchiaEmu/releases?redirect=example.com",
            "https://github.com/Likounsee/ArchiaEmu/releases#other-project",
        ] {
            assert!(
                super::validate_release_notes_url(url).is_err(),
                "untrusted release notes URL should be rejected: {url}"
            );
        }
        assert!(super::validate_release_notes_url(
            "https://github.com/Likounsee/ArchiaEmu/releases"
        )
        .is_ok());
        assert!(super::validate_release_notes_url(
            "https://github.com/Likounsee/ArchiaEmu/releases/tag/v0.2.0"
        )
        .is_ok());
    }

    #[test]
    fn rejects_insecure_urls_and_malformed_hashes() {
        let manifest: UpdateManifest = serde_json::from_str(
            r#"{"schema_version":1,"version":"0.2.0","release_notes_url":"http://example.com/notes","downloads":{}}"#,
        )
        .expect("manifest JSON should parse");
        assert!(manifest.validate().is_err());
        let leading_zero_version: UpdateManifest = serde_json::from_str(
            r#"{"schema_version":1,"version":"01.2.3","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{}}"#,
        )
        .expect("manifest JSON should parse");
        assert!(leading_zero_version.validate().is_err());

        for malformed in [
            "https://",
            "https:///example.com/notes",
            "https://user@example.com/notes",
            "https://example.com\\\\@attacker.invalid/notes",
            "https://example.com/notes\\\\\\n",
        ] {
            assert!(
                super::validate_https_url(malformed, "release notes").is_err(),
                "malformed HTTPS URL should be rejected: {malformed}"
            );
        }
    }

    #[test]
    fn rejects_unsupported_manifest_schema_and_invalid_download_metadata() {
        let unsupported: UpdateManifest = serde_json::from_str(
            r#"{"schema_version":2,"version":"0.2.0","release_notes_url":"https://github.com/Likounsee/ArchiaEmu/releases","downloads":{}}"#,
        )
        .expect("manifest JSON should parse");
        assert!(unsupported.validate().is_err());

        let valid_url = "https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app.exe";
        for (size, digest) in [
            (0, "a".repeat(64)),
            (1, "not-a-sha256-digest".to_owned()),
            (1, "g".repeat(64)),
        ] {
            let manifest: UpdateManifest = serde_json::from_value(serde_json::json!({
                "schema_version": 1,
                "version": "0.2.0",
                "release_notes_url": "https://github.com/Likounsee/ArchiaEmu/releases",
                "downloads": {
                    "windows-x86_64": {
                        "url": valid_url,
                        "size_bytes": size,
                        "sha256": digest
                    }
                }
            }))
            .expect("manifest JSON should parse");
            assert!(
                manifest.validate().is_err(),
                "invalid size or digest must be rejected"
            );
        }
    }

    #[test]
    fn rejects_untrusted_update_download_hosts() {
        assert!(validate_download_url(
            "https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app.exe"
        )
        .is_ok());
        assert!(validate_download_url("https://example.com/app.exe").is_err());
        assert!(validate_download_url("https://github.com/other/project/releases/download/v0.2.0/app.exe").is_err());
        assert!(validate_download_url("https://github.com/Likounsee/ArchiaEmu/releases/tag/v0.2.0").is_err());
        assert!(validate_download_url("http://github.com/app.exe").is_err());
        // User-info must not be stripped before checking the actual URL authority.
        assert!(validate_download_url("https://attacker@github.com/app.exe").is_err());
        assert!(validate_download_url("https://github.com:8443/app.exe").is_err());
        assert!(validate_download_url("https://github.com.evil.example/app.exe").is_err());
    }

    #[test]
    fn compares_numeric_versions_without_lexicographic_errors() {
        assert!(is_newer_version("0.10.0", "0.9.9"));
        assert!(is_newer_version("1.0.0", "0.99.99"));
        assert!(!is_newer_version("0.1.0", "0.1.0"));
        for malformed in [
            "invalid",
            "1..3",
            "1.2.3.4",
            "01.2.3",
            "1.02.3",
            "1.2.03",
            "+1.2.3",
            "1.2.-3",
            "1.2.3-beta",
            " 1.2.3",
        ] {
            assert!(
                !is_newer_version(malformed, "0.1.0"),
                "malformed version must not be considered newer: {malformed}"
            );
        }
    }

    #[test]
    fn update_staging_rejects_noncanonical_versions_before_network_access() {
        let download = super::UpdateDownload {
            url: "https://github.com/Likounsee/ArchiaEmu/releases/download/v0.2.0/app.exe"
                .to_owned(),
            size_bytes: 1,
            sha256: "a".repeat(64),
        };

        for malformed in ["", "01.2.3", "1.2", "1.2.3-beta", "1.2.3.4"] {
            assert!(
                super::download_and_verify(&download, malformed).is_err(),
                "invalid version must be rejected before downloading: {malformed}"
            );
        }
    }

    #[test]
    fn validates_http_content_length_before_staging() {
        assert!(validate_content_length(None, 10).is_ok());
        assert!(validate_content_length(Some("10"), 10).is_ok());
        assert!(validate_content_length(Some("9"), 10).is_err());
        assert!(validate_content_length(Some("not-a-number"), 10).is_err());
        assert!(validate_content_length(Some("-1"), 10).is_err());
    }

    #[test]
    fn staged_file_must_match_size_and_sha256() {
        let path = temporary_file();
        let contents = b"trusted staged update";
        write_test_file(&path, contents);
        let digest = format!("{:x}", Sha256::digest(contents));
        verify_staged_file(&path, contents.len() as u64, &digest)
            .expect("valid staged file should pass");
        assert!(verify_staged_file(&path, contents.len() as u64 + 1, &digest).is_err());
        assert!(verify_staged_file(&path, contents.len() as u64, &"0".repeat(64)).is_err());

        // A staged file can be changed after download; re-verification must catch it.
        write_test_file(&path, b"tampered update contents");
        assert!(verify_staged_file(&path, contents.len() as u64, &digest).is_err());
        let _ = std::fs::remove_file(path);
    }
}
