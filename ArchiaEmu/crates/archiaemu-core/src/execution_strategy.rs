#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Explicit CPU execution policy shared by console modules and host planning.
//!
//! Selecting a strategy does not implement a CPU, ABI bridge, JIT, or sandbox.
//! A console module must still advertise only the functionality it actually
//! implements and validate its own system requirements.

/// CPU instruction-set family reported by a console module or the host.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CpuArchitecture {
    /// 64-bit x86 instruction set.
    X86_64,
    /// 64-bit Arm instruction set.
    Aarch64,
    /// 32-bit Arm instruction set.
    Arm32,
    /// 32-bit PowerPC instruction set.
    PowerPc32,
    /// 64-bit PowerPC instruction set.
    PowerPc64,
    /// 32-bit MIPS instruction set.
    Mips32,
    /// 64-bit MIPS instruction set.
    Mips64,
    /// A target whose instruction set is not represented by a standard variant.
    Proprietary(&'static str),
}

/// CPU execution method requested for a console module.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CpuExecutionStrategy {
    /// Run compatible x86-64 guest code on an x86-64 host, with a separate
    /// console ABI, operating-system and system-service compatibility layer.
    NativeX64WithCompatibility,
    /// Interpret guest instructions without requiring a dynamic code generator.
    Interpreter,
    /// Translate guest instructions into host machine code at runtime.
    DynamicRecompiler,
}

/// Host-controlled preferences for planning CPU execution.
///
/// These preferences are not proof that a strategy has been implemented. The
/// selected module must confirm that it supports the strategy before launch.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CpuExecutionPolicy {
    /// Permit native x86-64 execution when both guest and host are x86-64.
    ///
    /// This must remain opt-in because matching instruction sets alone does
    /// not solve ABI, privilege, syscall, memory, or device compatibility.
    pub allow_native_x64_compatibility: bool,
    /// Prefer a JIT when native x86-64 compatibility cannot be selected.
    ///
    /// A module that has no JIT must use an interpreter instead.
    pub prefer_dynamic_recompiler: bool,
}

impl Default for CpuExecutionPolicy {
    fn default() -> Self {
        Self {
            allow_native_x64_compatibility: true,
            prefer_dynamic_recompiler: false,
        }
    }
}

/// Selects a preferred CPU strategy without claiming that the module implements it.
///
/// Native execution is selected only for an x86-64 guest on an x86-64 host and
/// only when explicitly permitted by policy. Other targets use the requested
/// JIT preference or fall back to the interpreter. A module must reject or
/// renegotiate a strategy it does not implement; callers must not silently
/// treat this planning result as a compatibility guarantee.
pub const fn select_cpu_execution_strategy(
    guest: CpuArchitecture,
    host: CpuArchitecture,
    policy: CpuExecutionPolicy,
) -> CpuExecutionStrategy {
    if matches!(guest, CpuArchitecture::X86_64)
        && matches!(host, CpuArchitecture::X86_64)
        && policy.allow_native_x64_compatibility
    {
        CpuExecutionStrategy::NativeX64WithCompatibility
    } else if policy.prefer_dynamic_recompiler {
        CpuExecutionStrategy::DynamicRecompiler
    } else {
        CpuExecutionStrategy::Interpreter
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn policy(native: bool, jit: bool) -> CpuExecutionPolicy {
        CpuExecutionPolicy {
            allow_native_x64_compatibility: native,
            prefer_dynamic_recompiler: jit,
        }
    }

    #[test]
    fn native_x64_requires_matching_architectures_and_explicit_permission() {
        assert_eq!(
            select_cpu_execution_strategy(
                CpuArchitecture::X86_64,
                CpuArchitecture::X86_64,
                policy(true, false)
            ),
            CpuExecutionStrategy::NativeX64WithCompatibility
        );
        assert_eq!(
            select_cpu_execution_strategy(
                CpuArchitecture::X86_64,
                CpuArchitecture::Aarch64,
                policy(true, false)
            ),
            CpuExecutionStrategy::Interpreter
        );
        assert_eq!(
            select_cpu_execution_strategy(
                CpuArchitecture::X86_64,
                CpuArchitecture::X86_64,
                policy(false, false)
            ),
            CpuExecutionStrategy::Interpreter
        );
    }

    #[test]
    fn non_x64_guests_use_interpreter_or_preferred_jit() {
        for guest in [
            CpuArchitecture::Aarch64,
            CpuArchitecture::Arm32,
            CpuArchitecture::PowerPc32,
            CpuArchitecture::PowerPc64,
            CpuArchitecture::Mips32,
            CpuArchitecture::Mips64,
            CpuArchitecture::Proprietary("custom"),
        ] {
            assert_eq!(
                select_cpu_execution_strategy(guest, CpuArchitecture::X86_64, policy(true, false)),
                CpuExecutionStrategy::Interpreter
            );
            assert_eq!(
                select_cpu_execution_strategy(guest, CpuArchitecture::X86_64, policy(true, true)),
                CpuExecutionStrategy::DynamicRecompiler
            );
        }
    }

    #[test]
    fn native_x64_takes_precedence_over_jit_preference_when_allowed() {
        assert_eq!(
            select_cpu_execution_strategy(
                CpuArchitecture::X86_64,
                CpuArchitecture::X86_64,
                policy(true, true)
            ),
            CpuExecutionStrategy::NativeX64WithCompatibility
        );
    }
}
