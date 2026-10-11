#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Dependency-aware planning for optional emulator modules.
//!
//! This crate-level contract intentionally does not download, execute, or
//! install packages. A trusted application layer can consume the deterministic
//! plan and perform platform-specific installation with integrity checks.

use core::fmt;

/// Stable, machine-readable identifier for a distributable module.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct ModuleId(&'static str);

impl ModuleId {
    /// Creates an ID using lowercase ASCII letters, digits, and single hyphens.
    pub const fn new(value: &'static str) -> Result<Self, ModuleIdError> {
        let bytes = value.as_bytes();
        if bytes.is_empty() {
            return Err(ModuleIdError::Empty);
        }
        let mut index = 0;
        let mut previous_hyphen = true;
        while index < bytes.len() {
            let byte = bytes[index];
            let valid =
                (byte >= b'a' && byte <= b'z') || (byte >= b'0' && byte <= b'9') || byte == b'-';
            if !valid {
                return Err(ModuleIdError::InvalidCharacter);
            }
            if byte == b'-' && previous_hyphen {
                return Err(ModuleIdError::InvalidSeparator);
            }
            previous_hyphen = byte == b'-';
            index += 1;
        }
        if previous_hyphen {
            return Err(ModuleIdError::InvalidSeparator);
        }
        Ok(Self(value))
    }

    /// Returns the canonical ID string.
    pub const fn as_str(self) -> &'static str {
        self.0
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
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ModuleDescriptor {
    /// Stable ID used by configuration and dependency declarations.
    pub id: ModuleId,
    /// Human-readable name displayed in the setup wizard.
    pub display_name: &'static str,
    /// Current readiness shown to users.
    pub readiness: ModuleReadiness,
    /// Direct module dependencies, which must be installed first.
    pub dependencies: &'static [ModuleId],
}

/// A validated catalogue of optional modules.
pub struct ModuleCatalog<'a> {
    descriptors: &'a [ModuleDescriptor],
}

/// Error detected while validating a catalogue.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CatalogError {
    /// Two descriptors use the same ID.
    DuplicateModule(ModuleId),
    /// A dependency points to an ID absent from the catalogue.
    MissingDependency {
        module: ModuleId,
        dependency: ModuleId,
    },
    /// A dependency path eventually refers back to a module already on that path.
    DependencyCycle(ModuleId),
}

impl fmt::Display for CatalogError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::DuplicateModule(id) => write!(f, "duplicate module '{}'", id.as_str()),
            Self::MissingDependency { module, dependency } => write!(
                f,
                "module '{}' requires missing module '{}'",
                module.as_str(),
                dependency.as_str()
            ),
            Self::DependencyCycle(id) => {
                write!(f, "module dependency cycle includes '{}'", id.as_str())
            }
        }
    }
}

impl std::error::Error for CatalogError {}

/// Error while resolving a user's requested installation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PlanError {
    /// The user requested a module absent from the catalogue.
    UnknownModule(ModuleId),
    /// A requested or required module is not ready for installation.
    NotInstallable {
        module: ModuleId,
        readiness: ModuleReadiness,
    },
    /// The catalogue must be validated before planning.
    InvalidCatalog(CatalogError),
}

impl fmt::Display for PlanError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownModule(id) => write!(f, "unknown module '{}'", id.as_str()),
            Self::NotInstallable { module, readiness } => write!(
                f,
                "module '{}' cannot be installed in state {:?}",
                module.as_str(),
                readiness
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
                return Err(CatalogError::DuplicateModule(descriptor.id));
            }
            for dependency in descriptor.dependencies {
                if !self.descriptors.iter().any(|item| item.id == *dependency) {
                    return Err(CatalogError::MissingDependency {
                        module: descriptor.id,
                        dependency: *dependency,
                    });
                }
            }
        }

        let mut visited = Vec::new();
        let mut visiting = Vec::new();
        for descriptor in self.descriptors {
            self.visit(descriptor.id, &mut visited, &mut visiting)?;
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
            if !self.descriptors.iter().any(|item| item.id == *id) {
                return Err(PlanError::UnknownModule(*id));
            }
            self.plan_visit(*id, installed, &mut visited, &mut visiting, &mut order)?;
        }
        Ok(ModuleInstallPlan {
            install_order: order,
        })
    }

    fn visit(
        &self,
        id: ModuleId,
        visited: &mut Vec<ModuleId>,
        visiting: &mut Vec<ModuleId>,
    ) -> Result<(), CatalogError> {
        if visited.contains(&id) {
            return Ok(());
        }
        if visiting.contains(&id) {
            return Err(CatalogError::DependencyCycle(id));
        }
        visiting.push(id);
        let descriptor = self
            .descriptor(id)
            .expect("validated dependency must exist");
        for dependency in descriptor.dependencies {
            self.visit(*dependency, visited, visiting)?;
        }
        visiting.pop();
        visited.push(id);
        Ok(())
    }

    fn plan_visit(
        &self,
        id: ModuleId,
        installed: &[ModuleId],
        visited: &mut Vec<ModuleId>,
        visiting: &mut Vec<ModuleId>,
        order: &mut Vec<ModuleId>,
    ) -> Result<(), PlanError> {
        if visited.contains(&id) {
            return Ok(());
        }
        if visiting.contains(&id) {
            return Err(PlanError::InvalidCatalog(CatalogError::DependencyCycle(id)));
        }
        visiting.push(id);
        let descriptor = self.descriptor(id).ok_or(PlanError::UnknownModule(id))?;
        if !installed.contains(&id) && descriptor.readiness == ModuleReadiness::InDevelopment {
            return Err(PlanError::NotInstallable {
                module: id,
                readiness: descriptor.readiness,
            });
        }
        for dependency in descriptor.dependencies {
            self.plan_visit(*dependency, installed, visited, visiting, order)?;
        }
        visiting.pop();
        visited.push(id);
        if !installed.contains(&id) {
            order.push(id);
        }
        Ok(())
    }

    fn descriptor(&self, id: ModuleId) -> Option<&ModuleDescriptor> {
        self.descriptors.iter().find(|item| item.id == id)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn id(value: &'static str) -> ModuleId {
        ModuleId::new(value).unwrap()
    }

    const SHARED: ModuleId = match ModuleId::new("shared-runtime") {
        Ok(value) => value,
        Err(_) => panic!("test ID must be valid"),
    };
    const GB: ModuleId = match ModuleId::new("game-boy") {
        Ok(value) => value,
        Err(_) => panic!("test ID must be valid"),
    };
    const NES: ModuleId = match ModuleId::new("nes") {
        Ok(value) => value,
        Err(_) => panic!("test ID must be valid"),
    };
    const EXPERIMENTAL: ModuleId = match ModuleId::new("experimental") {
        Ok(value) => value,
        Err(_) => panic!("test ID must be valid"),
    };
    const FUTURE: ModuleId = match ModuleId::new("future-system") {
        Ok(value) => value,
        Err(_) => panic!("test ID must be valid"),
    };

    const NO_DEPENDENCIES: &[ModuleId] = &[];
    const SHARED_DEPENDENCY: &[ModuleId] = &[SHARED];
    const CATALOG: &[ModuleDescriptor] = &[
        ModuleDescriptor {
            id: SHARED,
            display_name: "Shared runtime",
            readiness: ModuleReadiness::Installable,
            dependencies: NO_DEPENDENCIES,
        },
        ModuleDescriptor {
            id: GB,
            display_name: "Game Boy",
            readiness: ModuleReadiness::Experimental,
            dependencies: SHARED_DEPENDENCY,
        },
        ModuleDescriptor {
            id: NES,
            display_name: "NES",
            readiness: ModuleReadiness::Installable,
            dependencies: SHARED_DEPENDENCY,
        },
        ModuleDescriptor {
            id: EXPERIMENTAL,
            display_name: "Experimental",
            readiness: ModuleReadiness::InDevelopment,
            dependencies: NO_DEPENDENCIES,
        },
    ];

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
        let catalog = ModuleCatalog::new(CATALOG);
        let plan = catalog.plan_install(&[GB, NES], &[]).unwrap();
        assert_eq!(plan.install_order, vec![SHARED, GB, NES]);
    }

    #[test]
    fn omits_installed_modules_but_repairs_missing_dependencies() {
        let catalog = ModuleCatalog::new(CATALOG);
        let plan = catalog.plan_install(&[GB], &[GB]).unwrap();
        assert_eq!(plan.install_order, vec![SHARED]);
        let plan = catalog.plan_install(&[GB], &[SHARED]).unwrap();
        assert_eq!(plan.install_order, vec![GB]);
    }

    #[test]
    fn rejects_unknown_and_not_yet_implemented_modules() {
        let catalog = ModuleCatalog::new(CATALOG);
        assert_eq!(
            catalog.plan_install(&[FUTURE], &[]),
            Err(PlanError::UnknownModule(FUTURE))
        );
        assert_eq!(
            catalog.plan_install(&[EXPERIMENTAL], &[]),
            Err(PlanError::NotInstallable {
                module: EXPERIMENTAL,
                readiness: ModuleReadiness::InDevelopment,
            })
        );
    }

    #[test]
    fn rejects_duplicate_ids_and_missing_dependencies() {
        let duplicate = [
            ModuleDescriptor {
                id: SHARED,
                display_name: "A",
                readiness: ModuleReadiness::Installable,
                dependencies: NO_DEPENDENCIES,
            },
            ModuleDescriptor {
                id: SHARED,
                display_name: "B",
                readiness: ModuleReadiness::Installable,
                dependencies: NO_DEPENDENCIES,
            },
        ];
        assert_eq!(ModuleCatalog::new(&duplicate).validate(),
            Err(CatalogError::DuplicateModule(SHARED)));

        let missing = [ModuleDescriptor {
            id: GB,
            display_name: "Game Boy",
            readiness: ModuleReadiness::Installable,
            dependencies: &[FUTURE],
        }];
        assert_eq!(
            ModuleCatalog::new(&missing).validate(),
            Err(CatalogError::MissingDependency {
                module: GB,
                dependency: FUTURE,
            })
        );
    }

    #[test]
    fn rejects_dependency_cycles() {
        let a = id("cycle-a");
        let b = id("cycle-b");
        let descriptors = [
            ModuleDescriptor {
                id: a,
                display_name: "A",
                readiness: ModuleReadiness::Installable,
                dependencies: &[b],
            },
            ModuleDescriptor {
                id: b,
                display_name: "B",
                readiness: ModuleReadiness::Installable,
                dependencies: &[a],
            },
        ];
        assert_eq!(
            ModuleCatalog::new(&descriptors).validate(),
            Err(CatalogError::DependencyCycle(a))
        );
    }

    #[test]
    fn catalogue_validation_is_independent_of_selection() {
        assert!(ModuleCatalog::new(CATALOG).validate().is_ok());
    }
}
