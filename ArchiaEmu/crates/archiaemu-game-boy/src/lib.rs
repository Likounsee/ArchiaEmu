#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! One Game Boy emulator target for original Game Boy and Game Boy Color games.
//!
//! Cartridge metadata identifies whether a title is monochrome-only, compatible
//! with both hardware modes, or requires Color hardware. This is still an early
//! foundation: CPU execution, memory mapping, and peripherals remain incomplete.

/// LR35902 register state and flag-accurate arithmetic primitives.
pub mod cpu;

use cpu::{Cpu, CpuBus, RegisterPair};

/// The minimum image length needed to include the complete cartridge header.
pub const MINIMUM_HEADER_SIZE: usize = 0x150;

const NINTENDO_LOGO: [u8; 48] = [
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
];

/// Probes for a Game Boy cartridge using its required Nintendo logo.
///
/// A match only means that the identifying logo and minimum header are present;
/// the full header checksum, ROM length, mapper and execution support still
/// require their dedicated validation paths.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct GameBoyCartridgeProbe;

impl archiaemu_core::content::ContentFormatProbe for GameBoyCartridgeProbe {
    fn format_id(&self) -> &'static str {
        "game-boy"
    }

    fn probe(&self, prefix: &[u8]) -> archiaemu_core::content::FormatProbeResult {
        use archiaemu_core::content::FormatProbeResult;

        const LOGO_START: usize = 0x0104;
        const LOGO_END: usize = 0x0134;

        if prefix.len() < LOGO_END {
            return FormatProbeResult::NeedMoreData {
                minimum_bytes: LOGO_END,
            };
        }
        if prefix[LOGO_START..LOGO_END] != NINTENDO_LOGO {
            return FormatProbeResult::NoMatch;
        }
        if prefix.len() < MINIMUM_HEADER_SIZE {
            return FormatProbeResult::NeedMoreData {
                minimum_bytes: MINIMUM_HEADER_SIZE,
            };
        }
        FormatProbeResult::Match
    }
}

/// Metadata extracted from a Game Boy cartridge header.
///
/// Raw codes are retained rather than guessed into support claims. Mapper and
/// memory behavior must be implemented and validated separately.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CartridgeHeader {
    /// Raw 16-byte title field at 0x0134..=0x0143.
    pub title: [u8; 16],
    /// Raw cartridge hardware/mapper type code at 0x0147.
    pub cartridge_type: u8,
    /// Raw ROM size code at 0x0148.
    pub rom_size_code: u8,
    /// Raw external RAM size code at 0x0149.
    pub ram_size_code: u8,
    /// Raw CGB compatibility flag at 0x0143 (overlaps the title field).
    pub cgb_flag: u8,
    /// Raw header checksum byte at 0x014D.
    pub header_checksum: u8,
}

impl CartridgeHeader {
    /// Interprets the cartridge's Color-capability flag without guessing
    /// unsupported/reserved values.
    pub const fn compatibility(&self) -> CartridgeCompatibility {
        match self.cgb_flag {
            0x00 => CartridgeCompatibility::DmgOnly,
            0x80 => CartridgeCompatibility::DmgAndCgb,
            0xC0 => CartridgeCompatibility::CgbOnly,
            other => CartridgeCompatibility::Reserved(other),
        }
    }

    /// Chooses the single emulator's hardware mode for this cartridge.
    ///
    /// Dual-compatible and Color-only cartridges prefer Color hardware. A
    /// monochrome-only cartridge runs in original Game Boy mode. Reserved flags
    /// return None so callers can report an explicit unknown/unsupported case.
    pub const fn preferred_hardware_mode(&self) -> Option<HardwareMode> {
        match self.compatibility() {
            CartridgeCompatibility::DmgOnly => Some(HardwareMode::Dmg),
            CartridgeCompatibility::DmgAndCgb | CartridgeCompatibility::CgbOnly => {
                Some(HardwareMode::Cgb)
            }
            CartridgeCompatibility::Reserved(_) => None,
        }
    }

    /// Parses the header after validating image length, Nintendo logo, size code,
    /// and header checksum.
    ///
    /// This does not validate mapper support, the complete ROM length against its
    /// size code, or whether the image can be executed.
    pub fn parse(image: &[u8]) -> Result<Self, HeaderError> {
        if image.len() < MINIMUM_HEADER_SIZE {
            return Err(HeaderError::ImageTooShort {
                minimum: MINIMUM_HEADER_SIZE,
                actual: image.len(),
            });
        }

        if image[0x0104..0x0134] != NINTENDO_LOGO {
            return Err(HeaderError::NintendoLogoMismatch);
        }

        let rom_size_code = image[0x148];
        if !is_valid_rom_size_code(rom_size_code) {
            return Err(HeaderError::InvalidRomSizeCode(rom_size_code));
        }

        let ram_size_code = image[0x149];
        if !is_valid_ram_size_code(ram_size_code) {
            return Err(HeaderError::InvalidRamSizeCode(ram_size_code));
        }

        let expected = image[0x14D];
        let mut checksum = 0u8;
        for byte in &image[0x134..=0x14C] {
            checksum = checksum.wrapping_sub(*byte).wrapping_sub(1);
        }
        if checksum != expected {
            return Err(HeaderError::HeaderChecksumMismatch {
                expected,
                actual: checksum,
            });
        }

        let mut title = [0u8; 16];
        title.copy_from_slice(&image[0x134..=0x143]);

        Ok(Self {
            title,
            cartridge_type: image[0x147],
            rom_size_code,
            ram_size_code: image[0x149],
            cgb_flag: image[0x143],
            header_checksum: expected,
        })
    }
}

/// Failure while parsing a Game Boy cartridge header.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HeaderError {
    /// Image does not contain the complete header.
    ImageTooShort {
        /// Minimum required number of bytes.
        minimum: usize,
        /// Actual number of bytes supplied.
        actual: usize,
    },
    /// The 48-byte Nintendo logo in the cartridge header does not match the defined bytes.
    NintendoLogoMismatch,
    /// ROM size code is not one defined by the original Game Boy header format.
    InvalidRomSizeCode(u8),
    /// RAM size code is not one defined by the original Game Boy header format.
    InvalidRamSizeCode(u8),
    /// Header checksum differs from the checksum computed over 0x0134..=0x014C.
    HeaderChecksumMismatch {
        /// Checksum byte stored in the image.
        expected: u8,
        /// Checksum computed from the image.
        actual: u8,
    },
}

impl core::fmt::Display for HeaderError {
    fn fmt(&self, formatter: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::ImageTooShort { minimum, actual } => {
                write!(formatter, "cartridge image too short: need at least {minimum} bytes, got {actual}")
            }
            Self::NintendoLogoMismatch => {
                formatter.write_str("Game Boy cartridge Nintendo logo mismatch")
            }
            Self::InvalidRomSizeCode(code) => {
                write!(formatter, "invalid Game Boy ROM size code: 0x{code:02X}")
            }
            Self::InvalidRamSizeCode(code) => {
                write!(formatter, "invalid Game Boy RAM size code: 0x{code:02X}")
            }
            Self::HeaderChecksumMismatch { expected, actual } => write!(
                formatter,
                "Game Boy header checksum mismatch: stored 0x{expected:02X}, computed 0x{actual:02X}"
            ),
        }
    }
}

impl std::error::Error for HeaderError {}

const fn is_valid_rom_size_code(code: u8) -> bool {
    matches!(code, 0x00..=0x08 | 0x52..=0x54)
}

const fn is_valid_ram_size_code(code: u8) -> bool {
    matches!(code, 0x00..=0x05)
}

const fn expected_rom_size(code: u8) -> Option<usize> {
    match code {
        0x00..=0x08 => Some(0x8000usize << code),
        0x52 => Some(72 * 0x4000),
        0x53 => Some(80 * 0x4000),
        0x54 => Some(96 * 0x4000),
        _ => None,
    }
}

/// Compatibility declared by the cartridge's CGB flag.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CartridgeCompatibility {
    /// Game Boy monochrome hardware only.
    DmgOnly,
    /// Supports original Game Boy and Game Boy Color hardware.
    DmgAndCgb,
    /// Requires Game Boy Color hardware.
    CgbOnly,
    /// Reserved flag value; not treated as a supported compatibility claim.
    Reserved(u8),
}

/// Hardware behavior selected inside the unified Game Boy emulator.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HardwareMode {
    /// Original monochrome Game Boy behavior.
    Dmg,
    /// Game Boy Color behavior, including color hardware features.
    Cgb,
}

/// Stable identifier shared by Game Boy and Game Boy Color game support.
pub const SYSTEM_ID: &str = "game-boy";

/// A module adapter that exposes Game Boy cartridge inspection through the
/// shared ArchiaEmu core contract.
///
/// This is an early execution foundation, not a functioning game emulator yet:
/// a limited LR35902 instruction subset runs over a basic ROM/WRAM/HRAM bus.
/// Video, audio, I/O devices, timers, interrupts, input, mapper banking, and most
/// instructions remain unimplemented, so game compatibility is not claimed.
pub struct GameBoyCore {
    image: Option<Vec<u8>>,
    header: Option<CartridgeHeader>,
    cpu: Cpu,
    work_ram: [u8; 0x2000],
    high_ram: [u8; 0x7F],
    interrupt_enable: u8,
    interrupt_request: u8,
}

impl GameBoyCore {
    /// Creates an uninitialized unified Game Boy / Game Boy Color module instance.
    pub const fn new() -> Self {
        Self {
            image: None,
            header: None,
            cpu: Cpu::new(),
            work_ram: [0; 0x2000],
            high_ram: [0; 0x7F],
            interrupt_enable: 0,
            interrupt_request: 0,
        }
    }

    /// Returns validated cartridge metadata after content has been loaded.
    pub const fn cartridge_header(&self) -> Option<&CartridgeHeader> {
        self.header.as_ref()
    }

    /// Returns the hardware mode selected for the loaded cartridge, if known.
    ///
    /// The same core instance handles both modes; this value is not a separate
    /// emulator/module selection.
    pub const fn hardware_mode(&self) -> Option<HardwareMode> {
        match self.header.as_ref() {
            Some(header) => header.preferred_hardware_mode(),
            None => None,
        }
    }
}

impl Default for GameBoyCore {
    fn default() -> Self {
        Self::new()
    }
}

fn game_boy_core_descriptor() -> archiaemu_core::CoreDescriptor {
    archiaemu_core::CoreDescriptor {
        api_version: archiaemu_core::CURRENT_CORE_API_VERSION,
        system_id: archiaemu_core::SystemId::new(SYSTEM_ID)
            .expect("the built-in Game Boy system ID is valid"),
        display_name: "Game Boy / Game Boy Color",
        core_version: env!("CARGO_PKG_VERSION"),
        // Save states cover the complete mutable state of the currently implemented ROM-only machine.
        capabilities: archiaemu_core::CoreCapabilities {
            content_loading: true,
            save_states: true,
            ..archiaemu_core::CoreCapabilities::default()
        },
    }
}

impl archiaemu_core::EmulationCore for GameBoyCore {
    fn descriptor(&self) -> archiaemu_core::CoreDescriptor {
        game_boy_core_descriptor()
    }

    fn load_content(&mut self, content: &[u8]) -> Result<(), archiaemu_core::CoreError> {
        let header = CartridgeHeader::parse(content).map_err(|_| {
            archiaemu_core::CoreError::InvalidContent("invalid Game Boy cartridge header")
        })?;
        // Validate the declared image length for every mapper, even when
        // execution support for that mapper has not been implemented yet.
        let required_size = expected_rom_size(header.rom_size_code).ok_or(
            archiaemu_core::CoreError::InvalidContent("invalid ROM size code"),
        )?;
        if content.len() < required_size {
            return Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size",
            ));
        }
        // Commit state only after validation succeeds, preserving any previously
        // loaded image if the caller supplies malformed replacement content.
        self.image = Some(content.to_vec());
        self.header = Some(header);
        self.work_ram.fill(0);
        self.high_ram.fill(0);
        self.interrupt_enable = 0;
        self.interrupt_request = 0;
        self.cpu = Cpu::new();
        if header.cartridge_type == 0x00
            && header.preferred_hardware_mode().is_some()
            && matches!(
                expected_rom_size(header.rom_size_code),
                Some(required) if content.len() >= required
            )
        {
            // For complete supported ROM-only images, loading also establishes
            // the initial machine state so the runtime can execute immediately.
            self.reset()?;
        }
        Ok(())
    }

    fn reset(&mut self) -> Result<(), archiaemu_core::CoreError> {
        let header = self
            .header
            .as_ref()
            .ok_or(archiaemu_core::CoreError::NotInitialized)?;
        if header.cartridge_type != 0x00 {
            return Err(archiaemu_core::CoreError::Unsupported(
                "execution currently supports ROM-only cartridges (mapper 0)",
            ));
        }
        let required_size = expected_rom_size(header.rom_size_code).ok_or(
            archiaemu_core::CoreError::InvalidContent("invalid ROM size code"),
        )?;
        if self.image.as_ref().map_or(0, Vec::len) < required_size {
            return Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size",
            ));
        }

        self.cpu = Cpu::new();
        self.cpu.registers.pc = 0x0100;
        self.cpu.registers.sp = 0xFFFE;
        match header.preferred_hardware_mode() {
            Some(HardwareMode::Dmg) => {
                self.cpu.registers.a = 0x01;
                self.cpu.registers.set_f(0xB0);
                self.cpu.registers.set_pair(RegisterPair::Bc, 0x0013);
                self.cpu.registers.set_pair(RegisterPair::De, 0x00D8);
                self.cpu.registers.set_pair(RegisterPair::Hl, 0x014D);
            }
            Some(HardwareMode::Cgb) => {
                self.cpu.registers.a = 0x11;
                self.cpu.registers.set_f(0x80);
                self.cpu.registers.set_pair(RegisterPair::Bc, 0x0000);
                self.cpu.registers.set_pair(RegisterPair::De, 0xFF56);
                self.cpu.registers.set_pair(RegisterPair::Hl, 0x000D);
            }
            None => {
                return Err(archiaemu_core::CoreError::Unsupported(
                    "reserved Game Boy Color compatibility flag",
                ));
            }
        }
        self.work_ram.fill(0);
        self.high_ram.fill(0);
        self.interrupt_enable = 0;
        self.interrupt_request = 0;
        Ok(())
    }

    fn save_state(&self) -> Result<Vec<u8>, archiaemu_core::CoreError> {
        use archiaemu_core::CoreError;

        let image = self.image.as_deref().ok_or(CoreError::NotInitialized)?;
        let header = self.header.as_ref().ok_or(CoreError::NotInitialized)?;
        if header.cartridge_type != 0x00 {
            return Err(CoreError::Unsupported(
                "save states currently support ROM-only cartridges (mapper 0)",
            ));
        }
        let cpu = self.cpu.encode_save_state();
        let mut state = Vec::with_capacity(8_354);
        state.extend_from_slice(b"GBST");
        state.extend_from_slice(&1_u16.to_le_bytes());
        state.extend_from_slice(
            &u32::try_from(image.len())
                .map_err(|_| CoreError::InvalidState("ROM image is too large"))?
                .to_le_bytes(),
        );
        state.extend_from_slice(&rom_fingerprint(image).to_le_bytes());
        state.extend_from_slice(&cpu);
        state.extend_from_slice(&self.work_ram);
        state.extend_from_slice(&self.high_ram);
        state.push(self.interrupt_enable);
        state.push(self.interrupt_request);
        Ok(state)
    }

    fn load_state(&mut self, state: &[u8]) -> Result<(), archiaemu_core::CoreError> {
        use archiaemu_core::CoreError;

        const STATE_BYTES: usize = 8_354;
        const CPU_START: usize = 18;
        const CPU_END: usize = CPU_START + 15;
        const WRAM_END: usize = CPU_END + 0x2000;
        const HRAM_END: usize = WRAM_END + 0x7F;
        if state.len() != STATE_BYTES {
            return Err(CoreError::InvalidState("unexpected Game Boy state length"));
        }
        if &state[..4] != b"GBST" {
            return Err(CoreError::InvalidState("invalid Game Boy state signature"));
        }
        if u16::from_le_bytes([state[4], state[5]]) != 1 {
            return Err(CoreError::InvalidState(
                "unsupported Game Boy state version",
            ));
        }
        let image = self.image.as_deref().ok_or(CoreError::NotInitialized)?;
        let header = self.header.as_ref().ok_or(CoreError::NotInitialized)?;
        if header.cartridge_type != 0x00 {
            return Err(CoreError::Unsupported(
                "save states currently support ROM-only cartridges (mapper 0)",
            ));
        }
        let stored_image_len = u32::from_le_bytes(state[6..10].try_into().expect("fixed slice"));
        let stored_fingerprint = u64::from_le_bytes(state[10..18].try_into().expect("fixed slice"));
        if usize::try_from(stored_image_len).ok() != Some(image.len())
            || stored_fingerprint != rom_fingerprint(image)
        {
            return Err(CoreError::InvalidState(
                "save state belongs to a different ROM image",
            ));
        }
        let cpu =
            Cpu::decode_save_state(&state[CPU_START..CPU_END]).map_err(CoreError::InvalidState)?;
        let interrupt_enable = state[HRAM_END];
        let interrupt_request = state[HRAM_END + 1];
        if interrupt_enable & !0x1F != 0 || interrupt_request & !0x1F != 0 {
            return Err(CoreError::InvalidState("invalid interrupt register bits"));
        }

        // All validation is complete: commit the decoded machine state atomically.
        self.cpu = cpu;
        self.work_ram.copy_from_slice(&state[CPU_END..WRAM_END]);
        self.high_ram.copy_from_slice(&state[WRAM_END..HRAM_END]);
        self.interrupt_enable = interrupt_enable;
        self.interrupt_request = interrupt_request;
        Ok(())
    }

    fn run_until_event(&mut self) -> Result<archiaemu_core::CoreEvent, archiaemu_core::CoreError> {
        let image = self
            .image
            .as_deref()
            .ok_or(archiaemu_core::CoreError::NotInitialized)?;
        let header = self
            .header
            .as_ref()
            .ok_or(archiaemu_core::CoreError::NotInitialized)?;
        if header.cartridge_type != 0x00 {
            return Err(archiaemu_core::CoreError::Unsupported(
                "execution currently supports ROM-only cartridges (mapper 0)",
            ));
        }
        let required_size = expected_rom_size(header.rom_size_code).ok_or(
            archiaemu_core::CoreError::InvalidContent("invalid ROM size code"),
        )?;
        if image.len() < required_size {
            return Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size",
            ));
        }
        if header.preferred_hardware_mode().is_none() {
            return Err(archiaemu_core::CoreError::Unsupported(
                "reserved Game Boy Color compatibility flag",
            ));
        }

        let mut bus = GameBoyBus {
            image,
            work_ram: &mut self.work_ram,
            high_ram: &mut self.high_ram,
            interrupt_enable: &mut self.interrupt_enable,
            interrupt_request: &mut self.interrupt_request,
        };
        self.cpu.step(&mut bus).map_err(|_| {
            archiaemu_core::CoreError::Execution(
                "unsupported LR35902 opcode in current instruction subset",
            )
        })?;
        if self.cpu.is_halted() {
            Ok(archiaemu_core::CoreEvent::Halted)
        } else {
            Ok(archiaemu_core::CoreEvent::Yielded)
        }
    }
}

/// A deliberately limited memory bus for the first executable CPU milestone.
///
/// Cartridge banking, video RAM, OAM, I/O devices, timers, and interrupt
/// arbitration are not implemented. Unmapped or unavailable hardware reads as
/// 0xFF; only ROM, work RAM/echo RAM, and high RAM have backing storage.
struct GameBoyBus<'a> {
    image: &'a [u8],
    work_ram: &'a mut [u8; 0x2000],
    high_ram: &'a mut [u8; 0x7F],
    interrupt_enable: &'a mut u8,
    interrupt_request: &'a mut u8,
}

impl CpuBus for GameBoyBus<'_> {
    fn read8(&mut self, address: u16) -> u8 {
        match address {
            0x0000..=0x7FFF => self
                .image
                .get(usize::from(address))
                .copied()
                .unwrap_or(0xFF),
            0xC000..=0xDFFF => self.work_ram[usize::from(address - 0xC000)],
            0xE000..=0xFDFF => self.work_ram[usize::from(address - 0xE000)],
            0xFF0F => 0xE0 | *self.interrupt_request,
            0xFF80..=0xFFFE => self.high_ram[usize::from(address - 0xFF80)],
            0xFFFF => 0xE0 | *self.interrupt_enable,
            _ => 0xFF,
        }
    }

    fn write8(&mut self, address: u16, value: u8) {
        match address {
            0xC000..=0xDFFF => self.work_ram[usize::from(address - 0xC000)] = value,
            0xE000..=0xFDFF => self.work_ram[usize::from(address - 0xE000)] = value,
            0xFF0F => *self.interrupt_request = value & 0x1F,
            0xFF80..=0xFFFE => self.high_ram[usize::from(address - 0xFF80)] = value,
            0xFFFF => *self.interrupt_enable = value & 0x1F,
            // Cartridge ROM and currently unimplemented hardware regions ignore writes.
            _ => {}
        }
    }
}

/// Computes a stable content fingerprint used to reject states for another ROM.
fn rom_fingerprint(image: &[u8]) -> u64 {
    image.iter().fold(0xcbf29ce484222325_u64, |hash, byte| {
        (hash ^ u64::from(*byte)).wrapping_mul(0x100000001b3)
    })
}

/// Registers the single Game Boy / Game Boy Color module with the shared core registry.
///
/// Registration makes the module discoverable, but does not imply that game
/// execution is implemented. The descriptor deliberately reports no execution
/// capabilities until the corresponding hardware components are complete.
pub fn register_game_boy(
    registry: &mut archiaemu_core::CoreRegistry,
) -> Result<(), archiaemu_core::RegistryError> {
    registry.register_with_descriptor(game_boy_core_descriptor(), create_game_boy_core)
}

fn create_game_boy_core() -> Box<dyn archiaemu_core::EmulationCore> {
    Box::new(GameBoyCore::new())
}

#[cfg(test)]
mod tests {
    #[test]
    fn descriptor_advertises_cartridge_loading_without_claiming_media_or_input_support() {
        use archiaemu_core::EmulationCore;

        let capabilities = super::GameBoyCore::new().descriptor().capabilities;
        assert!(capabilities.content_loading);
        assert!(!capabilities.input);
        assert!(!capabilities.video_output);
        assert!(!capabilities.audio_output);
        assert!(capabilities.save_states);
        assert!(!capabilities.persistent_saves);
    }

    #[test]
    fn cartridge_probe_id_matches_registered_core_system_id() {
        use archiaemu_core::content::ContentFormatProbe;

        let mut registry = archiaemu_core::CoreRegistry::new();
        super::register_game_boy(&mut registry).unwrap();
        let system_id = registry.system_ids().next().unwrap();
        assert_eq!(super::GameBoyCartridgeProbe.format_id(), system_id.as_str());
    }

    #[test]
    fn cartridge_probe_requires_the_logo_and_complete_header() {
        use archiaemu_core::content::{ContentFormatProbe, FormatProbeResult};

        let probe = GameBoyCartridgeProbe;
        assert_eq!(
            probe.probe(&[]),
            FormatProbeResult::NeedMoreData {
                minimum_bytes: 0x0134,
            }
        );

        let mut image = vec![0; MINIMUM_HEADER_SIZE];
        image[0x0104..0x0134].copy_from_slice(&NINTENDO_LOGO);
        assert_eq!(
            probe.probe(&image[..0x0140]),
            FormatProbeResult::NeedMoreData {
                minimum_bytes: MINIMUM_HEADER_SIZE,
            }
        );
        assert_eq!(probe.probe(&image), FormatProbeResult::Match);

        image[0x0104] ^= 0xFF;
        assert_eq!(probe.probe(&image), FormatProbeResult::NoMatch);
    }

    #[test]
    fn cartridge_probe_match_does_not_bypass_header_validation() {
        use archiaemu_core::content::{ContentFormatProbe, FormatProbeResult};

        let probe = GameBoyCartridgeProbe;
        let mut image = vec![0; MINIMUM_HEADER_SIZE];
        image[0x0104..0x0134].copy_from_slice(&NINTENDO_LOGO);

        // Identification is intentionally weaker than structural validation:
        // the logo and length match, but this zero-filled header has a bad checksum.
        assert_eq!(probe.probe(&image), FormatProbeResult::Match);
        assert!(matches!(
            CartridgeHeader::parse(&image),
            Err(HeaderError::HeaderChecksumMismatch { .. })
        ));
    }

    #[test]
    fn cartridge_probe_works_with_the_generic_detector() {
        use archiaemu_core::content::{detect_content_formats, ContentDetection};

        let probe = GameBoyCartridgeProbe;
        let probes: [&dyn archiaemu_core::content::ContentFormatProbe; 1] = [&probe];
        let mut image = vec![0; MINIMUM_HEADER_SIZE];
        image[0x0104..0x0134].copy_from_slice(&NINTENDO_LOGO);
        assert_eq!(
            detect_content_formats(&image, &probes),
            ContentDetection {
                matches: vec!["game-boy"],
                needs_more_data: vec![],
            }
        );
    }

    use super::*;
    use archiaemu_core::EmulationCore;

    fn valid_image() -> Vec<u8> {
        let mut image = vec![0; MINIMUM_HEADER_SIZE];
        image[0x0104..0x0134].copy_from_slice(&NINTENDO_LOGO);
        image[0x134..0x13B].copy_from_slice(b"ARCHIA!");
        image[0x143] = 0x80;
        image[0x147] = 0x00;
        image[0x148] = 0x00;
        image[0x149] = 0x00;
        let checksum = header_checksum(&image);
        image[0x14D] = checksum;
        image
    }

    fn valid_rom() -> Vec<u8> {
        let mut image = valid_image();
        image.resize(0x8000, 0);
        image
    }

    fn header_checksum(image: &[u8]) -> u8 {
        image[0x134..=0x14C].iter().fold(0u8, |checksum, byte| {
            checksum.wrapping_sub(*byte).wrapping_sub(1)
        })
    }

    #[test]
    fn parses_valid_header_without_claiming_emulation_support() {
        let image = valid_image();
        let header = CartridgeHeader::parse(&image).unwrap();
        assert_eq!(&header.title[..7], b"ARCHIA!");
        assert_eq!(header.cartridge_type, 0x00);
        assert_eq!(header.rom_size_code, 0x00);
        assert_eq!(header.ram_size_code, 0x00);
        assert_eq!(header.cgb_flag, 0x80);
    }

    #[test]
    fn rejects_truncated_images() {
        assert_eq!(
            CartridgeHeader::parse(&[0; MINIMUM_HEADER_SIZE - 1]),
            Err(HeaderError::ImageTooShort {
                minimum: MINIMUM_HEADER_SIZE,
                actual: MINIMUM_HEADER_SIZE - 1,
            })
        );
    }

    #[test]
    fn rejects_all_header_truncation_boundaries() {
        for actual in [
            0, 1, 0x0103, 0x0104, 0x0133, 0x0134, 0x014C, 0x014D, 0x014E, 0x014F,
        ] {
            assert_eq!(
                CartridgeHeader::parse(&vec![0; actual]),
                Err(HeaderError::ImageTooShort {
                    minimum: MINIMUM_HEADER_SIZE,
                    actual,
                }),
                "truncated header length {actual:#X} must be rejected"
            );
        }
    }

    #[test]
    fn rejects_nintendo_logo_mismatch() {
        let mut image = valid_image();
        image[0x0104] ^= 0xFF;
        assert_eq!(
            CartridgeHeader::parse(&image),
            Err(HeaderError::NintendoLogoMismatch)
        );
    }

    #[test]
    fn rejects_invalid_rom_size_codes() {
        let mut image = valid_image();
        image[0x148] = 0x09;
        image[0x14D] = header_checksum(&image);
        assert_eq!(
            CartridgeHeader::parse(&image),
            Err(HeaderError::InvalidRomSizeCode(0x09))
        );
    }

    #[test]
    fn rejects_invalid_ram_size_codes() {
        for code in [0x06, 0x07, 0xFF] {
            let mut image = valid_image();
            image[0x149] = code;
            image[0x14D] = header_checksum(&image);
            assert_eq!(
                CartridgeHeader::parse(&image),
                Err(HeaderError::InvalidRamSizeCode(code))
            );
        }
    }

    #[test]
    fn accepts_defined_ram_size_codes() {
        for code in 0x00..=0x05 {
            let mut image = valid_image();
            image[0x149] = code;
            image[0x14D] = header_checksum(&image);
            assert_eq!(CartridgeHeader::parse(&image).unwrap().ram_size_code, code);
        }
    }

    #[test]
    fn rejects_header_checksum_mismatch() {
        let mut image = valid_image();
        image[0x14D] ^= 0xFF;
        assert!(matches!(
            CartridgeHeader::parse(&image),
            Err(HeaderError::HeaderChecksumMismatch { .. })
        ));
    }

    #[test]
    fn expected_rom_size_covers_all_defined_codes_and_rejects_reserved() {
        for code in 0x00..=0x08 {
            assert_eq!(
                expected_rom_size(code),
                Some(0x8000usize << code),
                "standard ROM size code {code:#04X}"
            );
        }
        assert_eq!(expected_rom_size(0x52), Some(72 * 0x4000));
        assert_eq!(expected_rom_size(0x53), Some(80 * 0x4000));
        assert_eq!(expected_rom_size(0x54), Some(96 * 0x4000));
        for code in [0x09, 0x0A, 0x51, 0x55, 0xFF] {
            assert_eq!(
                expected_rom_size(code),
                None,
                "reserved ROM size code {code:#04X}"
            );
        }
    }

    #[test]
    fn truncated_special_size_rom_does_not_replace_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        let mut replacement = valid_image();
        replacement[0x148] = 0x52;
        replacement[0x14D] = header_checksum(&replacement);
        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &replacement),
            Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn truncated_unsupported_mapper_does_not_replace_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        let mut replacement = valid_image();
        replacement[0x147] = 0x01;
        replacement[0x148] = 0x01;
        replacement[0x14D] = header_checksum(&replacement);
        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &replacement),
            Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn accepts_special_rom_size_codes() {
        for code in [0x52, 0x53, 0x54] {
            let mut image = valid_image();
            image[0x148] = code;
            image[0x14D] = header_checksum(&image);
            assert_eq!(CartridgeHeader::parse(&image).unwrap().rom_size_code, code);
        }
    }

    #[test]
    fn checksum_uses_wrapping_arithmetic() {
        let mut image = valid_image();
        image[0x134] = 0xFF;
        image[0x14D] = header_checksum(&image);
        assert!(CartridgeHeader::parse(&image).is_ok());
    }

    #[test]
    fn game_boy_is_discoverable_through_the_shared_registry() {
        let mut registry = archiaemu_core::CoreRegistry::new();
        register_game_boy(&mut registry).unwrap();

        let id = archiaemu_core::SystemId::new(SYSTEM_ID).unwrap();
        assert!(registry.contains(id));
        assert_eq!(
            registry.registered_descriptor(id),
            Some(game_boy_core_descriptor())
        );
        assert!(!registry.contains(archiaemu_core::SystemId::new("game-boy-dmg").unwrap()));
        assert!(!registry.contains(archiaemu_core::SystemId::new("game-boy-color").unwrap()));
        let core = registry.create(id).unwrap();
        assert_eq!(core.descriptor().display_name, "Game Boy / Game Boy Color");
        assert_eq!(
            core.descriptor().capabilities,
            archiaemu_core::CoreCapabilities {
                content_loading: true,
                save_states: true,
                ..archiaemu_core::CoreCapabilities::default()
            }
        );
    }

    #[test]
    fn module_validates_complete_rom_and_executes_the_first_instruction() {
        let mut core = GameBoyCore::new();
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Err(archiaemu_core::CoreError::NotInitialized)
        );

        let image = valid_rom();
        archiaemu_core::EmulationCore::load_content(&mut core, &image).unwrap();
        assert_eq!(
            core.cartridge_header().unwrap().cartridge_type,
            image[0x147]
        );
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
        assert_eq!(core.cpu.registers.pc, 0x0101);
    }

    #[test]
    fn save_state_round_trip_restores_cpu_and_all_implemented_ram() {
        use archiaemu_core::EmulationCore;

        let mut core = super::GameBoyCore::new();
        let image = valid_rom();
        core.load_content(&image).unwrap();
        core.cpu.registers.a = 0xA5;
        core.cpu.registers.set_f(0xB0);
        core.cpu.registers.sp = 0xCDEF;
        core.cpu.registers.pc = 0x4567;
        core.work_ram[0x123] = 0x91;
        core.work_ram[0x1FFF] = 0x27;
        core.high_ram[0x22] = 0x63;
        core.interrupt_enable = 0x12;
        core.interrupt_request = 0x08;

        let saved = core.save_state().unwrap();
        core.cpu.registers.a = 0;
        core.cpu.registers.sp = 0;
        core.cpu.registers.pc = 0;
        core.work_ram.fill(0);
        core.high_ram.fill(0);
        core.interrupt_enable = 0;
        core.interrupt_request = 0;

        core.load_state(&saved).unwrap();
        assert_eq!(core.cpu.registers.a, 0xA5);
        assert_eq!(core.cpu.registers.f(), 0xB0);
        assert_eq!(core.cpu.registers.sp, 0xCDEF);
        assert_eq!(core.cpu.registers.pc, 0x4567);
        assert_eq!(core.work_ram[0x123], 0x91);
        assert_eq!(core.work_ram[0x1FFF], 0x27);
        assert_eq!(core.high_ram[0x22], 0x63);
        assert_eq!(core.interrupt_enable, 0x12);
        assert_eq!(core.interrupt_request, 0x08);
    }

    #[test]
    fn invalid_or_foreign_save_state_does_not_mutate_machine_state() {
        use archiaemu_core::EmulationCore;

        let mut core = super::GameBoyCore::new();
        core.load_content(&valid_rom()).unwrap();
        core.work_ram[0] = 0x5A;
        let saved = core.save_state().unwrap();

        let mut corrupt = saved.clone();
        corrupt[0] ^= 0xFF;
        assert!(core.load_state(&corrupt).is_err());
        assert_eq!(core.work_ram[0], 0x5A);

        let mut other_rom = valid_rom();
        other_rom[0x0134] ^= 1;
        other_rom[0x014D] = header_checksum(&other_rom);
        let mut other_core = super::GameBoyCore::new();
        other_core.load_content(&other_rom).unwrap();
        assert!(other_core.load_state(&saved).is_err());
        assert!(other_core.work_ram.iter().all(|byte| *byte == 0));
    }

    #[test]
    fn execution_rejects_unimplemented_cartridge_mappers() {
        let mut core = GameBoyCore::new();
        let mut image = valid_rom();
        image[0x147] = 0x01;
        image[0x14D] = header_checksum(&image);
        archiaemu_core::EmulationCore::load_content(&mut core, &image).unwrap();
        assert_eq!(
            archiaemu_core::EmulationCore::reset(&mut core),
            Err(archiaemu_core::CoreError::Unsupported(
                "execution currently supports ROM-only cartridges (mapper 0)"
            ))
        );
    }

    #[test]
    fn one_core_selects_hardware_mode_from_cartridge_flag() {
        let mut image = valid_image();
        let header = CartridgeHeader::parse(&image).unwrap();
        assert_eq!(header.compatibility(), CartridgeCompatibility::DmgAndCgb);
        assert_eq!(header.preferred_hardware_mode(), Some(HardwareMode::Cgb));

        image[0x143] = 0x00;
        image[0x14D] = header_checksum(&image);
        let header = CartridgeHeader::parse(&image).unwrap();
        assert_eq!(header.compatibility(), CartridgeCompatibility::DmgOnly);
        assert_eq!(header.preferred_hardware_mode(), Some(HardwareMode::Dmg));

        image[0x143] = 0xC0;
        image[0x14D] = header_checksum(&image);
        let header = CartridgeHeader::parse(&image).unwrap();
        assert_eq!(header.compatibility(), CartridgeCompatibility::CgbOnly);
        assert_eq!(header.preferred_hardware_mode(), Some(HardwareMode::Cgb));
    }

    #[test]
    fn reserved_color_flag_is_not_silently_guessed() {
        let mut image = valid_image();
        image[0x143] = 0x40;
        image[0x14D] = header_checksum(&image);
        let header = CartridgeHeader::parse(&image).unwrap();
        assert_eq!(
            header.compatibility(),
            CartridgeCompatibility::Reserved(0x40)
        );
        assert_eq!(header.preferred_hardware_mode(), None);
    }

    #[test]
    fn unified_core_selects_mode_without_changing_system_identity() {
        let mut core = GameBoyCore::new();
        assert_eq!(core.hardware_mode(), None);

        let color_compatible_image = valid_rom();
        archiaemu_core::EmulationCore::load_content(&mut core, &color_compatible_image).unwrap();
        assert_eq!(core.hardware_mode(), Some(HardwareMode::Cgb));
        assert_eq!(
            core.descriptor().system_id,
            archiaemu_core::SystemId::new("game-boy").unwrap()
        );

        let mut monochrome_image = valid_rom();
        monochrome_image[0x143] = 0x00;
        monochrome_image[0x14D] = header_checksum(&monochrome_image);
        archiaemu_core::EmulationCore::load_content(&mut core, &monochrome_image).unwrap();
        assert_eq!(core.hardware_mode(), Some(HardwareMode::Dmg));
        assert_eq!(
            core.descriptor().system_id,
            archiaemu_core::SystemId::new("game-boy").unwrap()
        );
    }

    #[test]
    fn rejects_truncated_rom_before_replacing_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &valid_image()),
            Err(archiaemu_core::CoreError::InvalidContent(
                "ROM image is shorter than its declared size"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn invalid_ram_size_replacement_preserves_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        let mut replacement = valid_rom();
        replacement[0x149] = 0x06;
        replacement[0x14D] = header_checksum(&replacement);
        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &replacement),
            Err(archiaemu_core::CoreError::InvalidContent(
                "invalid Game Boy cartridge header"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn invalid_checksum_replacement_preserves_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        let mut replacement = valid_rom();
        replacement[0x134] ^= 0x01;
        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &replacement),
            Err(archiaemu_core::CoreError::InvalidContent(
                "invalid Game Boy cartridge header"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn invalid_logo_replacement_preserves_loaded_cartridge() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        let mut replacement = valid_rom();
        replacement[0x0104] ^= 0xFF;
        assert_eq!(
            archiaemu_core::EmulationCore::load_content(&mut core, &replacement),
            Err(archiaemu_core::CoreError::InvalidContent(
                "invalid Game Boy cartridge header"
            ))
        );
        assert_eq!(core.cartridge_header(), Some(&before));
        assert_eq!(
            archiaemu_core::EmulationCore::run_until_event(&mut core),
            Ok(archiaemu_core::CoreEvent::Yielded)
        );
    }

    #[test]
    fn invalid_replacement_rom_does_not_destroy_previous_metadata() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        assert!(archiaemu_core::EmulationCore::load_content(&mut core, &valid_image()).is_err());
        assert_eq!(core.cartridge_header(), Some(&before));
    }
}
