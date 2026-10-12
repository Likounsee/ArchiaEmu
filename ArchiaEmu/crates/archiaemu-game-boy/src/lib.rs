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
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
];


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

impl archiaemu_core::EmulationCore for GameBoyCore {
    fn descriptor(&self) -> archiaemu_core::CoreDescriptor {
        archiaemu_core::CoreDescriptor {
            api_version: archiaemu_core::CURRENT_CORE_API_VERSION,
            system_id: archiaemu_core::SystemId::new(SYSTEM_ID)
                .expect("the built-in Game Boy system ID is valid"),
            display_name: "Game Boy / Game Boy Color",
            core_version: env!("CARGO_PKG_VERSION"),
            // Do not advertise execution capabilities before they exist.
            capabilities: archiaemu_core::CoreCapabilities::default(),
        }
    }

    fn load_content(&mut self, content: &[u8]) -> Result<(), archiaemu_core::CoreError> {
        let header = CartridgeHeader::parse(content).map_err(|_| {
            archiaemu_core::CoreError::InvalidContent("invalid Game Boy cartridge header")
        })?;
        if header.cartridge_type == 0x00 {
            let required_size = expected_rom_size(header.rom_size_code).ok_or(
                archiaemu_core::CoreError::InvalidContent("invalid ROM size code"),
            )?;
            if content.len() < required_size {
                return Err(archiaemu_core::CoreError::InvalidContent(
                    "ROM image is shorter than its declared size",
                ));
            }
        }
        // Commit state only after validation succeeds, preserving any previously
        // loaded image if the caller supplies malformed replacement content.
        self.image = Some(content.to_vec());
        self.header = Some(header);
        self.work_ram.fill(0);
        self.high_ram.fill(0);
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
            0xFF80..=0xFFFE => self.high_ram[usize::from(address - 0xFF80)],
            _ => 0xFF,
        }
    }

    fn write8(&mut self, address: u16, value: u8) {
        match address {
            0xC000..=0xDFFF => self.work_ram[usize::from(address - 0xC000)] = value,
            0xE000..=0xFDFF => self.work_ram[usize::from(address - 0xE000)] = value,
            0xFF80..=0xFFFE => self.high_ram[usize::from(address - 0xFF80)] = value,
            // Cartridge ROM and currently unimplemented hardware regions ignore writes.
            _ => {}
        }
    }
}

/// Registers the single Game Boy / Game Boy Color module with the shared core registry.
///
/// Registration makes the module discoverable, but does not imply that game
/// execution is implemented. The descriptor deliberately reports no execution
/// capabilities until the corresponding hardware components are complete.
pub fn register_game_boy(
    registry: &mut archiaemu_core::CoreRegistry,
) -> Result<(), archiaemu_core::RegistryError> {
    registry.register(
        archiaemu_core::SystemId::new(SYSTEM_ID).expect("the built-in Game Boy system ID is valid"),
        create_game_boy_core,
    )
}

fn create_game_boy_core() -> Box<dyn archiaemu_core::EmulationCore> {
    Box::new(GameBoyCore::new())
}

#[cfg(test)]
mod tests {
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
    fn rejects_header_checksum_mismatch() {
        let mut image = valid_image();
        image[0x14D] ^= 0xFF;
        assert!(matches!(
            CartridgeHeader::parse(&image),
            Err(HeaderError::HeaderChecksumMismatch { .. })
        ));
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
        assert!(!registry.contains(archiaemu_core::SystemId::new("game-boy-dmg").unwrap()));
        assert!(!registry.contains(archiaemu_core::SystemId::new("game-boy-color").unwrap()));
        let core = registry.create(id).unwrap();
        assert_eq!(core.descriptor().display_name, "Game Boy / Game Boy Color");
        assert_eq!(
            core.descriptor().capabilities,
            archiaemu_core::CoreCapabilities::default()
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
    fn execution_rejects_unimplemented_cartridge_mappers() {
        let mut core = GameBoyCore::new();
        let mut image = valid_image();
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
    fn invalid_replacement_rom_does_not_destroy_previous_metadata() {
        let mut core = GameBoyCore::new();
        archiaemu_core::EmulationCore::load_content(&mut core, &valid_rom()).unwrap();
        let before = *core.cartridge_header().unwrap();

        assert!(archiaemu_core::EmulationCore::load_content(&mut core, &valid_image()).is_err());
        assert_eq!(core.cartridge_header(), Some(&before));
    }
}
