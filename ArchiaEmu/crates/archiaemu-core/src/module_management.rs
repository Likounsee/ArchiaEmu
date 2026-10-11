#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Dependency-aware planning for optional emulator modules.
//!
//! This crate-level contract intentionally contains no network, installer, or
//! platform-specific operations. A trusted application layer can consume the
//! deterministic plan and perform verified package installation.

use core::fmt;

/// Stable, machine-readable identifier for a distributable module.
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct ModuleId(String);

impl ModuleId {
    /// Creates an ID using lowercase ASCII letters, digits, and single hyphens.
    pub fn new(value: impl Into<String>) -> Result<Self, ModuleIdError> {
        let value = value.into();
        let bytes = value.as_bytes();
        if bytes.is_empty() {
            return Err(ModuleIdError::Empty);
        }
        let mut previous_hyphen = true;
        for byte in bytes {
            let valid = (*byte >= b'a' && *byte <= b'z')
                || (*byte >= b'0' && *byte <= b'9')
                || *byte == b'-';
            if !valid {
                return Err(ModuleIdError::InvalidCharacter);
            }
            if *byte == b'-' && previous_hyphen {
                return Err(ModuleIdError::InvalidSeparator);
            }
            previous_hyphen = *byte == b'-';
        }
        if previous_hyphen {
            return Err(ModuleIdError::InvalidSeparator);
        }
        Ok(Self(value))
    }

    /// Returns the canonical ID string.
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl fmt::Display for ModuleId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

/// Error while validating a module ID.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ModuleIdError {
    /// The ID is empty.
    Empty,
    /// The ID contains unsupported characters.
    InvalidCharacter,
    /// The ID starts or ends with a hyphen or contains consecutive hyphens.
    InvalidSeparator,
}

impl fmt::Display for ModuleIdError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Empty => f.write_str("module ID cannot be empty"),
            Self::InvalidCharacter => {
                f.write_str("module ID must contain lowercase ASCII letters, digits, and hyphens")
            }
            Self::InvalidSeparator => f.write_str("module ID has an invalid hyphen separator"),
        }
    }
}

impl std::error::Error for ModuleIdError {}

/// Compatibility/readiness status presented by the module catalogue.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ModuleReadiness {
    /// The module is available for normal installation.
    Installable,
    /// The module can be installed, but its functionality is not yet mature.
    Experimental,
    /// The module is not installable because implementation work is incomplete.
    InDevelopment,
}

/// Immutable catalogue metadata for one module package.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ModuleDescriptor {
    /// Stable ID used by configuration and dependency declarations.
    pub id: ModuleId,
    /// Human-readable name displayed in the setup wizard.
    pub display_name: String,
    /// Current readiness shown to users.
    pub readiness: ModuleReadiness,
    /// Direct module dependencies, which must be installed first.
    pub dependencies: Vec<ModuleId>,
}

/// A validated catalogue of optional modules.
pub struct ModuleCatalog<'a> {
    descriptors: &'a [ModuleDescriptor],
}

/// Error detected while validating a catalogue.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CatalogError {
    /// Two descriptors use the same ID.
    DuplicateModule(ModuleId),
    /// A dependency points to an ID absent from the catalogue.
    MissingDependency {
        /// Module declaring the dependency.
        module: ModuleId,
        /// Dependency ID absent from the catalogue.
        dependency: ModuleId,
    },
    /// A dependency path eventually refers back to a module already on that path.
    DependencyCycle(ModuleId),
}

impl fmt::Display for CatalogError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::DuplicateModule(id) => write!(f, "duplicate module '{}'", id),
            Self::MissingDependency { module, dependency } => write!(
                f,
                "module '{}' requires missing module '{}'",
                module, dependency
            ),
            Self::DependencyCycle(id) => {
                write!(f, "module dependency cycle includes '{}'", id)
            }
        }
    }
}

impl std::error::Error for CatalogError {}

/// Error while resolving a user's requested installation.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum PlanError {
    /// The user requested a module absent from the catalogue.
    UnknownModule(ModuleId),
    /// A requested or required module is not ready for installation.
    NotInstallable {
        /// Module requested directly or required by another module.
        module: ModuleId,
        /// Readiness state that prevents installation.
        readiness: ModuleReadiness,
    },
    /// The catalogue must be validated before planning.
    InvalidCatalog(CatalogError),
}

impl fmt::Display for PlanError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownModule(id) => write!(f, "unknown module '{}'", id),
            Self::NotInstallable { module, readiness } => write!(
                f,
                "module '{}' cannot be installed in state {:?}",
                module, readiness
            ),
            Self::InvalidCatalog(error) => write!(f, "invalid module catalogue: {error}"),
        }
    }
}

impl std::error::Error for PlanError {}

/// Deterministic installation order with dependencies before dependants.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ModuleInstallPlan {
    /// Modules to install in order. Already-installed modules are omitted.
    pub install_order: Vec<ModuleId>,
}

impl<'a> ModuleCatalog<'a> {
    /// Creates a catalogue view over immutable descriptors.
    pub const fn new(descriptors: &'a [ModuleDescriptor]) -> Self {
        Self { descriptors }
    }

    /// Returns all catalogue descriptors in their declared order.
    pub fn descriptors(&self) -> &'a [ModuleDescriptor] {
        self.descriptors
    }

    /// Checks uniqueness, dependency references, and dependency cycles.
    pub fn validate(&self) -> Result<(), CatalogError> {
        for (index, descriptor) in self.descriptors.iter().enumerate() {
            if self.descriptors[..index]
                .iter()
                .any(|prior| prior.id == descriptor.id)
            {
                return Err(CatalogError::DuplicateModule(descriptor.id.clone()));
            }
            for dependency in &descriptor.dependencies {
                if self.descriptor(dependency).is_none() {
                    return Err(CatalogError::MissingDependency {
                        module: descriptor.id.clone(),
                        dependency: dependency.clone(),
                    });
                }
            }
        }

        let mut visited = Vec::new();
        let mut visiting = Vec::new();
        for descriptor in self.descriptors {
            self.visit(&descriptor.id, &mut visited, &mut visiting)?;
        }
        Ok(())
    }

    /// Resolves requested modules and transitive dependencies into install order.
    ///
    /// The result is deterministic: dependencies precede dependants, catalogue
    /// declaration order breaks ties, and IDs already listed as installed are
    /// not emitted. Missing installed dependencies are still planned.
    pub fn plan_install(
        &self,
        requested: &[ModuleId],
        installed: &[ModuleId],
    ) -> Result<ModuleInstallPlan, PlanError> {
        self.validate().map_err(PlanError::InvalidCatalog)?;
        let mut visited = Vec::new();
        let mut visiting = Vec::new();
        let mut order = Vec::new();
        for id in requested {
            if self.descriptor(id).is_none() {
                return Err(PlanError::UnknownModule(id.clone()));
            }
            self.plan_visit(id, installed, &mut visited, &mut visiting, &mut order)?;
        }
        Ok(ModuleInstallPlan {
            install_order: order,
        })
    }

    fn visit(
        &self,
        id: &ModuleId,
        visited: &mut Vec<ModuleId>,
        visiting: &mut Vec<ModuleId>,
    ) -> Result<(), CatalogError> {
        if visited.contains(id) {
            return Ok(());
        }
        if visiting.contains(id) {
            return Err(CatalogError::DependencyCycle(id.clone()));
        }
        visiting.push(id.clone());
        let descriptor = self
            .descriptor(id)
            .expect("validated dependency must exist");
        for dependency in &descriptor.dependencies {
            self.visit(dependency, visited, visiting)?;
        }
        visiting.pop();
        visited.push(id.clone());
        Ok(())
    }

    fn plan_visit(
        &self,
        id: &ModuleId,
        installed: &[ModuleId],
        visited: &mut Vec<ModuleId>,
        visiting: &mut Vec<ModuleId>,
        order: &mut Vec<ModuleId>,
    ) -> Result<(), PlanError> {
        if visited.contains(id) {
            return Ok(());
        }
        if visiting.contains(id) {
            return Err(PlanError::InvalidCatalog(CatalogError::DependencyCycle(
                id.clone(),
            )));
        }
        visiting.push(id.clone());
        let descriptor = self
            .descriptor(id)
            .ok_or_else(|| PlanError::UnknownModule(id.clone()))?;
        if !installed.contains(id) && descriptor.readiness == ModuleReadiness::InDevelopment {
            return Err(PlanError::NotInstallable {
                module: id.clone(),
                readiness: descriptor.readiness,
            });
        }
        for dependency in &descriptor.dependencies {
            self.plan_visit(dependency, installed, visited, visiting, order)?;
        }
        visiting.pop();
        visited.push(id.clone());
        if !installed.contains(id) {
            order.push(id.clone());
        }
        Ok(())
    }

    fn descriptor(&self, id: &ModuleId) -> Option<&ModuleDescriptor> {
        self.descriptors.iter().find(|item| &item.id == id)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn id(value: &str) -> ModuleId {
        ModuleId::new(value).unwrap()
    }

    fn descriptor(
        id_value: &str,
        display_name: &str,
        readiness: ModuleReadiness,
        dependencies: &[&str],
    ) -> ModuleDescriptor {
        ModuleDescriptor {
            id: id(id_value),
            display_name: display_name.to_owned(),
            readiness,
            dependencies: dependencies.iter().map(|value| id(value)).collect(),
        }
    }

    fn catalogue_descriptors() -> Vec<ModuleDescriptor> {
        vec![
            descriptor(
                "shared-runtime",
                "Shared runtime",
                ModuleReadiness::Installable,
                &[],
            ),
            descriptor(
                "game-boy",
                "Game Boy",
                ModuleReadiness::Experimental,
                &["shared-runtime"],
            ),
            descriptor(
                "nes",
                "NES",
                ModuleReadiness::Installable,
                &["shared-runtime"],
            ),
            descriptor(
                "experimental",
                "Experimental",
                ModuleReadiness::InDevelopment,
                &[],
            ),
        ]
    }

    #[test]
    fn validates_module_ids() {
        assert_eq!(ModuleId::new(""), Err(ModuleIdError::Empty));
        assert_eq!(
            ModuleId::new("Game-Boy"),
            Err(ModuleIdError::InvalidCharacter)
        );
        assert_eq!(ModuleId::new("-gb"), Err(ModuleIdError::InvalidSeparator));
        assert_eq!(
            ModuleId::new("gb--color"),
            Err(ModuleIdError::InvalidSeparator)
        );
        assert_eq!(id("game-boy").as_str(), "game-boy");
    }

    #[test]
    fn plans_shared_dependencies_once_and_before_requested_modules() {
        let descriptors = catalogue_descriptors();
        let catalog = ModuleCatalog::new(&descriptors);
        let plan = catalog
            .plan_install(&[id("game-boy"), id("nes")], &[])
            .unwrap();
        assert_eq!(
            plan.install_order,
            vec![id("shared-runtime"), id("game-boy"), id("nes")]
        );
    }

    #[test]
    fn omits_installed_modules_but_repairs_missing_dependencies() {
        let descriptors = catalogue_descriptors();
        let catalog = ModuleCatalog::new(&descriptors);
        let plan = catalog
            .plan_install(&[id("game-boy")], &[id("game-boy")])
            .unwrap();
        assert_eq!(plan.install_order, vec![id("shared-runtime")]);
        let plan = catalog
            .plan_install(&[id("game-boy")], &[id("shared-runtime")])
            .unwrap();
        assert_eq!(plan.install_order, vec![id("game-boy")]);
    }

    #[test]
    fn rejects_unknown_and_not_yet_implemented_modules() {
        let descriptors = catalogue_descriptors();
        let catalog = ModuleCatalog::new(&descriptors);
        assert_eq!(
            catalog.plan_install(&[id("future-system")], &[]),
            Err(PlanError::UnknownModule(id("future-system")))
        );
        assert_eq!(
            catalog.plan_install(&[id("experimental")], &[]),
            Err(PlanError::NotInstallable {
                module: id("experimental"),
                readiness: ModuleReadiness::InDevelopment,
            })
        );
    }

    #[test]
    fn rejects_duplicate_ids_and_missing_dependencies() {
        let duplicate = vec![
            descriptor("shared-runtime", "A", ModuleReadiness::Installable, &[]),
            descriptor("shared-runtime", "B", ModuleReadiness::Installable, &[]),
        ];
        assert_eq!(
            ModuleCatalog::new(&duplicate).validate(),
            Err(CatalogError::DuplicateModule(id("shared-runtime")))
        );

        let missing = vec![descriptor(
            "game-boy",
            "Game Boy",
            ModuleReadiness::Installable,
            &["future-system"],
        )];
        assert_eq!(
            ModuleCatalog::new(&missing).validate(),
            Err(CatalogError::MissingDependency {
                module: id("game-boy"),
                dependency: id("future-system"),
            })
        );
    }

    #[test]
    fn rejects_dependency_cycles() {
        let descriptors = vec![
            descriptor("cycle-a", "A", ModuleReadiness::Installable, &["cycle-b"]),
            descriptor("cycle-b", "B", ModuleReadiness::Installable, &["cycle-a"]),
        ];
        assert_eq!(
            ModuleCatalog::new(&descriptors).validate(),
            Err(CatalogError::DependencyCycle(id("cycle-a")))
        );
    }

    #[test]
    fn catalogue_validation_is_independent_of_selection() {
        let descriptors = catalogue_descriptors();
        assert!(ModuleCatalog::new(&descriptors).validate().is_ok());
    }
}
