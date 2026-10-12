#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Versioned, platform-aware validation for remote module catalogue metadata.
//! This module does not fetch manifests, authenticate signatures, download, or install code.

use core::cmp::Ordering;
use core::fmt;
use crate::module_management::{ModuleId, ModuleReadiness};

/// Current supported schema version for module manifests.
pub const SUPPORTED_MANIFEST_SCHEMA: u32 = 1;

/// Strict numeric version with exactly three canonical decimal components.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ReleaseVersion {
    /// Major version.
    pub major: u64,
    /// Minor version.
    pub minor: u64,
    /// Patch version.
    pub patch: u64,
}

impl ReleaseVersion {
    /// Parses exactly major.minor.patch; leading zeroes are rejected except for zero itself.
    pub fn parse(value: &str) -> Result<Self, VersionParseError> {
        let mut parts = value.split('.');
        let major = parse_component(parts.next().ok_or(VersionParseError)?)?;
        let minor = parse_component(parts.next().ok_or(VersionParseError)?)?;
        let patch = parse_component(parts.next().ok_or(VersionParseError)?)?;
        if parts.next().is_some() { return Err(VersionParseError); }
        Ok(Self { major, minor, patch })
    }
}

fn parse_component(value: &str) -> Result<u64, VersionParseError> {
    if value.is_empty()
        || (value.len() > 1 && value.starts_with('0'))
        || !value.bytes().all(|byte| byte.is_ascii_digit())
    { return Err(VersionParseError); }
    value.parse().map_err(|_| VersionParseError)
}

impl Ord for ReleaseVersion {
    fn cmp(&self, other: &Self) -> Ordering {
        (self.major, self.minor, self.patch).cmp(&(other.major, other.minor, other.patch))
    }
}
impl PartialOrd for ReleaseVersion {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> { Some(self.cmp(other)) }
}
impl fmt::Display for ReleaseVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

/// Error parsing a strict numeric release version.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct VersionParseError;
impl fmt::Display for VersionParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("version must be canonical major.minor.patch")
    }
}
impl std::error::Error for VersionParseError {}

/// Operating system targeted by a module artifact.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TargetOs {
    /// Microsoft Windows.
    Windows,
    /// Apple macOS.
    Macos,
    /// Linux.
    Linux,
}

/// CPU architecture targeted by a module artifact.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TargetArch {
    /// 64-bit x86.
    X86_64,
    /// 64-bit ARM.
    Aarch64,
}

/// A host target; compatibility is explicit rather than inferred.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ModuleTarget {
    /// Operating system.
    pub os: TargetOs,
    /// CPU architecture.
    pub arch: TargetArch,
}

/// Metadata for an immutable downloadable package artifact.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PackageArtifact {
    /// HTTPS URL to the artifact.
    pub url: String,
    /// Exact artifact size in bytes.
    pub size_bytes: u64,
    /// Expected SHA-256 digest as 64 lowercase hexadecimal characters.
    pub sha256: String,
    /// Identifier of the trusted signing key expected by the installer.
    pub signing_key_id: String,
}

/// A module record in a versioned manifest.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ManifestModule {
    /// Stable module ID.
    pub id: ModuleId,
    /// Package version.
    pub version: ReleaseVersion,
    /// Minimum compatible ArchiaEmu version, inclusive.
    pub min_app_version: ReleaseVersion,
    /// First incompatible ArchiaEmu version, exclusive.
    pub max_app_version_exclusive: Option<ReleaseVersion>,
    /// Explicitly supported operating-system and CPU combinations.
    pub targets: Vec<ModuleTarget>,
    /// User-facing implementation readiness.
    pub readiness: ModuleReadiness,
    /// Direct module dependencies.
    pub dependencies: Vec<ModuleId>,
    /// Artifact metadata; development-only modules must not publish an artifact.
    pub artifact: Option<PackageArtifact>,
}

/// Parsed module catalogue document.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ModuleManifest {
    /// Schema version used to interpret this document.
    pub schema_version: u32,
    /// Monotonically increasing catalogue revision.
    pub revision: u64,
    /// Entries in display order.
    pub modules: Vec<ManifestModule>,
}

/// Error detected while validating a manifest.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ManifestError {
    /// Manifest schema is not supported.
    UnsupportedSchema(u32),
    /// Revision zero is invalid.
    InvalidRevision,
    /// Two entries use the same module ID.
    DuplicateModule(ModuleId),
    /// A dependency is absent from the manifest.
    MissingDependency { /// Module declaring the dependency. 
        module: ModuleId, /// Missing dependency ID.
        dependency: ModuleId },
    /// Dependency graph contains a cycle.
    DependencyCycle(ModuleId),
    /// Compatibility bounds contradict each other.
    InvalidCompatibilityRange(ModuleId),
    /// Installable or experimental entry lacks artifact metadata.
    MissingArtifact(ModuleId),
    /// Development-only entry advertises an artifact.
    ArtifactForInDevelopmentModule(ModuleId),
    /// Artifact URL, size, digest or signing-key ID is invalid.
    InvalidArtifact(ModuleId),
    /// A target appears more than once for a module.
    DuplicateTarget(ModuleId),
    /// Module has no artifact for the selected host target.
    UnsupportedTarget(ModuleId),
    /// Application version is outside the module compatibility range.
    IncompatibleApplicationVersion(ModuleId),
}

impl fmt::Display for ManifestError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnsupportedSchema(v) => write!(f, "unsupported manifest schema {v}"),
            Self::InvalidRevision => f.write_str("manifest revision must be greater than zero"),
            Self::DuplicateModule(id) => write!(f, "duplicate module '{id}'"),
            Self::MissingDependency { module, dependency } => write!(f, "module '{module}' requires missing module '{dependency}'"),
            Self::DependencyCycle(id) => write!(f, "module dependency cycle includes '{id}'"),
            Self::InvalidCompatibilityRange(id) => write!(f, "module '{id}' has an invalid application compatibility range"),
            Self::MissingArtifact(id) => write!(f, "module '{id}' requires artifact metadata"),
            Self::ArtifactForInDevelopmentModule(id) => write!(f, "in-development module '{id}' cannot publish an installable artifact"),
            Self::InvalidArtifact(id) => write!(f, "module '{id}' has invalid artifact metadata"),
            Self::DuplicateTarget(id) => write!(f, "module '{id}' declares a duplicate target"),
            Self::UnsupportedTarget(id) => write!(f, "module '{id}' has no artifact for the current host target"),
            Self::IncompatibleApplicationVersion(id) => write!(f, "module '{id}' is incompatible with this ArchiaEmu version"),
        }
    }
}
impl std::error::Error for ManifestError {}

impl ModuleManifest {
    /// Validates schema, IDs, dependency references, version ranges and artifact metadata.
    /// This is structural validation only; callers must authenticate the manifest separately.
    pub fn validate(&self) -> Result<(), ManifestError> {
        if self.schema_version != SUPPORTED_MANIFEST_SCHEMA {
            return Err(ManifestError::UnsupportedSchema(self.schema_version));
        }
        if self.revision == 0 { return Err(ManifestError::InvalidRevision); }

        for (index, module) in self.modules.iter().enumerate() {
            if self.modules[..index].iter().any(|prior| prior.id == module.id) {
                return Err(ManifestError::DuplicateModule(module.id.clone()));
            }
            if module.max_app_version_exclusive.is_some_and(|max| max <= module.min_app_version) {
                return Err(ManifestError::InvalidCompatibilityRange(module.id.clone()));
            }
            for (target_index, target) in module.targets.iter().enumerate() {
                if module.targets[..target_index].contains(target) {
                    return Err(ManifestError::DuplicateTarget(module.id.clone()));
                }
            }
            match (&module.artifact, module.readiness) {
                (None, ModuleReadiness::Installable | ModuleReadiness::Experimental) =>
                    return Err(ManifestError::MissingArtifact(module.id.clone())),
                (Some(_), ModuleReadiness::InDevelopment) =>
                    return Err(ManifestError::ArtifactForInDevelopmentModule(module.id.clone())),
                (Some(artifact), _) => {
                    let valid_url = artifact.url.starts_with("https://")
                        && artifact.url.len() > "https://".len()
                        && !artifact.url.bytes().any(|b| b.is_ascii_control() || b.is_ascii_whitespace());
                    let valid_digest = artifact.sha256.len() == 64
                        && artifact.sha256.bytes().all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b));
                    if !valid_url || artifact.size_bytes == 0 || !valid_digest || artifact.signing_key_id.trim().is_empty() {
                        return Err(ManifestError::InvalidArtifact(module.id.clone()));
                    }
                }
                (None, ModuleReadiness::InDevelopment) => {}
            }
            for dependency in &module.dependencies {
                if !self.modules.iter().any(|candidate| &candidate.id == dependency) {
                    return Err(ManifestError::MissingDependency {
                        module: module.id.clone(), dependency: dependency.clone()
                    });
                }
            }
        }
        let mut visited = Vec::new();
        let mut visiting = Vec::new();
        for module in &self.modules { self.visit(&module.id, &mut visited, &mut visiting)?; }
        Ok(())
    }

    /// Validates the manifest and filters entries for an explicit host and application version.
    /// In-development entries are omitted. A compatibility error is returned rather than silently
    /// presenting an incompatible package as available.
    pub fn modules_for_target(
        &self,
        app_version: ReleaseVersion,
        target: ModuleTarget,
    ) -> Result<Vec<&ManifestModule>, ManifestError> {
        self.validate()?;
        let mut compatible = Vec::new();
        for module in &self.modules {
            if module.readiness == ModuleReadiness::InDevelopment { continue; }
            if app_version < module.min_app_version
                || module.max_app_version_exclusive.is_some_and(|max| app_version >= max)
            { return Err(ManifestError::IncompatibleApplicationVersion(module.id.clone())); }
            if !module.targets.contains(&target) {
                return Err(ManifestError::UnsupportedTarget(module.id.clone()));
            }
            compatible.push(module);
        }
        Ok(compatible)
    }

    fn visit(&self, id: &ModuleId, visited: &mut Vec<ModuleId>, visiting: &mut Vec<ModuleId>) -> Result<(), ManifestError> {
        if visited.contains(id) { return Ok(()); }
        if visiting.contains(id) { return Err(ManifestError::DependencyCycle(id.clone())); }
        visiting.push(id.clone());
        let module = self.modules.iter().find(|m| &m.id == id)
            .expect("dependency existence was validated before graph traversal");
        for dependency in &module.dependencies { self.visit(dependency, visited, visiting)?; }
        visiting.pop();
        visited.push(id.clone());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn id(value: &str) -> ModuleId { ModuleId::new(value).unwrap() }
    fn target() -> ModuleTarget { ModuleTarget { os: TargetOs::Linux, arch: TargetArch::X86_64 } }
    fn artifact() -> PackageArtifact {
        PackageArtifact { url: "https://packages.example.invalid/module.tar.zst".into(), size_bytes: 1024, sha256: "a".repeat(64), signing_key_id: "release-2026".into() }
    }
    fn module(name: &str, deps: &[&str]) -> ManifestModule {
        ManifestModule {
            id: id(name), version: ReleaseVersion::parse("1.2.3").unwrap(),
            min_app_version: ReleaseVersion::parse("0.1.0").unwrap(),
            max_app_version_exclusive: Some(ReleaseVersion::parse("2.0.0").unwrap()),
            targets: vec![target()], readiness: ModuleReadiness::Installable,
            dependencies: deps.iter().map(|v| id(v)).collect(), artifact: Some(artifact()),
        }
    }
    fn manifest(modules: Vec<ManifestModule>) -> ModuleManifest {
        ModuleManifest { schema_version: SUPPORTED_MANIFEST_SCHEMA, revision: 1, modules }
    }

    #[test]
    fn parses_and_orders_numeric_versions() {
        let a = ReleaseVersion::parse("1.9.0").unwrap();
        let b = ReleaseVersion::parse("1.10.0").unwrap();
        assert!(a < b);
        assert_eq!(b.to_string(), "1.10.0");
        for bad in ["", "1", "1.2", "01.2.3", "1.02.3", "1.2.3.4", "v1.2.3", "1.-2.3"] {
            assert!(ReleaseVersion::parse(bad).is_err(), "accepted {bad}");
        }
    }

    #[test]
    fn validates_manifest_and_filters_by_target_and_app_version() {
        let m = manifest(vec![module("shared-runtime", &[]), module("game-boy", &["shared-runtime"])]);
        let selected = m.modules_for_target(ReleaseVersion::parse("1.5.0").unwrap(), target()).unwrap();
        assert_eq!(selected.len(), 2);
        assert_eq!(selected[1].id.as_str(), "game-boy");
        assert!(matches!(m.modules_for_target(ReleaseVersion::parse("2.0.0").unwrap(), target()), Err(ManifestError::IncompatibleApplicationVersion(_))));
    }

    #[test]
    fn rejects_duplicate_ids_missing_dependencies_and_cycles() {
        assert!(matches!(manifest(vec![module("one", &[]), module("one", &[])]).validate(), Err(ManifestError::DuplicateModule(_))));
        assert!(matches!(manifest(vec![module("one", &["missing"])]).validate(), Err(ManifestError::MissingDependency { .. })));
        assert!(matches!(manifest(vec![module("one", &["two"]), module("two", &["one"])]).validate(), Err(ManifestError::DependencyCycle(_))));
    }

    #[test]
    fn rejects_invalid_artifacts() {
        let mut entry = module("game-boy", &[]);
        entry.artifact.as_mut().unwrap().url = "http://example.invalid/file".into();
        assert!(matches!(manifest(vec![entry]).validate(), Err(ManifestError::InvalidArtifact(_))));
        let mut entry = module("game-boy", &[]);
        entry.artifact.as_mut().unwrap().sha256 = "not-a-hash".into();
        assert!(matches!(manifest(vec![entry]).validate(), Err(ManifestError::InvalidArtifact(_))));
        let mut entry = module("game-boy", &[]);
        entry.artifact = None;
        assert!(matches!(manifest(vec![entry]).validate(), Err(ManifestError::MissingArtifact(_))));
    }

    #[test]
    fn rejects_duplicate_targets_and_artifacts_for_development_modules() {
        let mut entry = module("game-boy", &[]);
        entry.targets.push(target());
        assert!(matches!(manifest(vec![entry]).validate(), Err(ManifestError::DuplicateTarget(_))));
        let mut entry = module("future-console", &[]);
        entry.readiness = ModuleReadiness::InDevelopment;
        assert!(matches!(manifest(vec![entry]).validate(), Err(ManifestError::ArtifactForInDevelopmentModule(_))));
    }

    #[test]
    fn in_development_entries_are_not_installable_choices() {
        let mut entry = module("future-console", &[]);
        entry.readiness = ModuleReadiness::InDevelopment;
        entry.artifact = None;
        assert!(manifest(vec![entry]).modules_for_target(ReleaseVersion::parse("1.0.0").unwrap(), target()).unwrap().is_empty());
    }
}
