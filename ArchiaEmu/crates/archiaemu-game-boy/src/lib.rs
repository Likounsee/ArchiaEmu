#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Defensive parsing of the original Game Boy cartridge header.
//!
//! This crate does not yet emulate the CPU, memory bus, or peripherals. A valid
//! parsed header is metadata only and does not imply that a cartridge can run.

/// The minimum image length needed to include the complete cartridge header.
pub const MINIMUM_HEADER_SIZE: usize = 0x150;

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
    /// Parses the header after validating image length, size code, and checksum.
    ///
    /// This does not validate the Nintendo logo, mapper support, the complete
    /// ROM length against its size code, or whether the image can be executed.
    pub fn parse(image: &[u8]) -> Result<Self, HeaderError> {
        if image.len() < MINIMUM_HEADER_SIZE {
            return Err(HeaderError::ImageTooShort {
                minimum: MINIMUM_HEADER_SIZE,
                actual: image.len(),
            });
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


/// Stable identifier for the original monochrome Game Boy (DMG) module.
pub const SYSTEM_ID: &str = "game-boy-dmg";

/// A module adapter that exposes Game Boy cartridge inspection through the
/// shared ArchiaEmu core contract.
///
/// This is intentionally not advertised as a functioning emulator yet:
/// CPU execution, memory mapping, video, audio, interrupts, and input remain
/// unimplemented. The adapter accepts and validates a ROM for inspection, but
/// execution methods return an explicit unsupported error.
pub struct GameBoyDmgCore {
    image: Option<Vec<u8>>,
    header: Option<CartridgeHeader>,
}

impl GameBoyDmgCore {
    /// Creates an uninitialized Game Boy DMG module instance.
    pub const fn new() -> Self {
        Self {
            image: None,
            header: None,
        }
    }

    /// Returns validated cartridge metadata after content has been loaded.
    pub const fn cartridge_header(&self) -> Option<&CartridgeHeader> {
        self.header.as_ref()
    }
}

impl Default for GameBoyDmgCore {
    fn default() -> Self {
        Self::new()
    }
}

impl archiaemu_core::EmulationCore for GameBoyDmgCore {
    fn descriptor(&self) -> archiaemu_core::CoreDescriptor {
        archiaemu_core::CoreDescriptor {
            system_id: archiaemu_core::SystemId::new(SYSTEM_ID)
                .expect("the built-in Game Boy system ID is valid"),
            display_name: "Nintendo Game Boy (DMG)",
            core_version: env!("CARGO_PKG_VERSION"),
            // Do not advertise execution capabilities before they exist.
            capabilities: archiaemu_core::CoreCapabilities::default(),
        }
    }

    fn load_content(&mut self, content: &[u8]) -> Result<(), archiaemu_core::CoreError> {
        let header = CartridgeHeader::parse(content).map_err(|_| {
            archiaemu_core::CoreError::InvalidContent("invalid Game Boy cartridge header")
        })?;
        // Commit state only after validation succeeds, preserving any previously
        // loaded image if the caller supplies malformed replacement content.
        self.image = Some(content.to_vec());
        self.header = Some(header);
        Ok(())
    }

    fn reset(&mut self) -> Result<(), archiaemu_core::CoreError> {
        if self.image.is_none() {
            return Err(archiaemu_core::CoreError::NotInitialized);
        }
        Err(archiaemu_core::CoreError::Unsupported(
            "Game Boy CPU execution is not implemented yet",
        ))
    }

    fn run_until_event(&mut self) -> Result<archiaemu_core::CoreEvent, archiaemu_core::CoreError> {
        if self.image.is_none() {
            return Err(archiaemu_core::CoreError::NotInitialized);
        }
        Err(archiaemu_core::CoreError::Unsupported(
            "Game Boy CPU execution is not implemented yet",
        ))
    }
}

/// Registers the Game Boy DMG module with the shared core registry.
///
/// Registration makes the module discoverable, but does not imply that game
/// execution is implemented. The descriptor deliberately reports no execution
/// capabilities until the corresponding hardware components are complete.
pub fn register_game_boy(
    registry: &mut archiaemu_core::CoreRegistry,
) -> Result<(), archiaemu_core::RegistryError> {
    registry.register(
        archiaemu_core::SystemId::new(SYSTEM_ID)
            .expect("the built-in Game Boy system ID is valid"),
        create_game_boy_core,
    )
}

fn create_game_boy_core() -> Box<dyn archiaemu_core::EmulationCore> {
    Box::new(GameBoyDmgCore::new())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn valid_image() -> Vec<u8> {
        let mut image = vec![0; MINIMUM_HEADER_SIZE];
        image[0x134..0x13B].copy_from_slice(b"ARCHIA!");
        image[0x143] = 0x80;
        image[0x147] = 0x00;
        image[0x148] = 0x00;
        image[0x149] = 0x00;
        let checksum = header_checksum(&image);
        image[0x14D] = checksum;
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
}
