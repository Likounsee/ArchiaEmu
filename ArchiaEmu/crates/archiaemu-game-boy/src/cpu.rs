#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! LR35902 CPU register state and flag-accurate arithmetic primitives.
//!
//! Instruction fetching, bus access, interrupt dispatch, and cycle scheduling
//! are intentionally not implemented here yet. These primitives are kept pure
//! so they can be exhaustively tested before being wired into execution.

/// LR35902 condition-flag bit masks in the upper nibble of register F.
pub mod flags {
    /// Zero result flag.
    pub const ZERO: u8 = 0x80;
    /// Subtraction flag.
    pub const SUBTRACT: u8 = 0x40;
    /// Half-carry / half-borrow flag.
    pub const HALF_CARRY: u8 = 0x20;
    /// Carry / borrow flag.
    pub const CARRY: u8 = 0x10;
}

/// The eight 8-bit registers and two 16-bit registers of an LR35902 CPU.
///
/// Register F is always masked to its upper nibble, matching the hardware's
/// requirement that its lower four bits read as zero.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct CpuRegisters {
    /// Accumulator register A.
    pub a: u8,
    /// Flag register F; its lower nibble is always zero.
    f: u8,
    /// General-purpose register B.
    pub b: u8,
    /// General-purpose register C.
    pub c: u8,
    /// General-purpose register D.
    pub d: u8,
    /// General-purpose register E.
    pub e: u8,
    /// General-purpose register H.
    pub h: u8,
    /// General-purpose register L.
    pub l: u8,
    /// Stack pointer.
    pub sp: u16,
    /// Program counter.
    pub pc: u16,
}

impl CpuRegisters {
    /// Returns the F register with its hardware-reserved lower nibble cleared.
    pub const fn f(&self) -> u8 {
        self.f & 0xF0
    }

    /// Sets the F register, ignoring writes to its reserved lower nibble.
    pub fn set_f(&mut self, value: u8) {
        self.f = value & 0xF0;
    }

    /// Returns whether the requested condition flag is set.
    pub const fn flag(&self, flag: u8) -> bool {
        self.f & flag != 0
    }

    /// Sets or clears one or more condition flags.
    ///
    /// Only the four documented upper-nibble flag bits are accepted.
    pub fn set_flag(&mut self, flag: u8, set: bool) {
        let mask = flag & 0xF0;
        if set {
            self.f |= mask;
        } else {
            self.f &= !mask;
        }
        self.f &= 0xF0;
    }

    /// Reads a 16-bit register pair in big-endian register order.
    pub const fn pair(&self, pair: RegisterPair) -> u16 {
        match pair {
            RegisterPair::Af => u16::from_be_bytes([self.a, self.f()]),
            RegisterPair::Bc => u16::from_be_bytes([self.b, self.c]),
            RegisterPair::De => u16::from_be_bytes([self.d, self.e]),
            RegisterPair::Hl => u16::from_be_bytes([self.h, self.l]),
        }
    }

    /// Writes a 16-bit register pair, preserving F's reserved-bit invariant.
    pub fn set_pair(&mut self, pair: RegisterPair, value: u16) {
        let [high, low] = value.to_be_bytes();
        match pair {
            RegisterPair::Af => {
                self.a = high;
                self.set_f(low);
            }
            RegisterPair::Bc => {
                self.b = high;
                self.c = low;
            }
            RegisterPair::De => {
                self.d = high;
                self.e = low;
            }
            RegisterPair::Hl => {
                self.h = high;
                self.l = low;
            }
        }
    }

    /// Increments an 8-bit value and updates Z, N and H, preserving C.
    pub fn increment8(&mut self, value: u8) -> u8 {
        let result = value.wrapping_add(1);
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, false);
        self.set_flag(flags::HALF_CARRY, value & 0x0F == 0x0F);
        result
    }

    /// Decrements an 8-bit value and updates Z, N and H, preserving C.
    pub fn decrement8(&mut self, value: u8) -> u8 {
        let result = value.wrapping_sub(1);
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, true);
        self.set_flag(flags::HALF_CARRY, value & 0x0F == 0);
        result
    }

    /// Adds an 8-bit value to A and updates all arithmetic flags.
    pub fn add_a(&mut self, value: u8) -> u8 {
        let a = self.a;
        let (result, carry) = a.overflowing_add(value);
        self.a = result;
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, false);
        self.set_flag(flags::HALF_CARRY, (a & 0x0F) + (value & 0x0F) > 0x0F);
        self.set_flag(flags::CARRY, carry);
        result
    }

    /// Subtracts an 8-bit value from A and updates all arithmetic flags.
    pub fn subtract_a(&mut self, value: u8) -> u8 {
        let a = self.a;
        let (result, borrow) = a.overflowing_sub(value);
        self.a = result;
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, true);
        self.set_flag(flags::HALF_CARRY, (a & 0x0F) < (value & 0x0F));
        self.set_flag(flags::CARRY, borrow);
        result
    }
}

/// Register pair selector used by 16-bit register accessors.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RegisterPair {
    /// Accumulator and flags.
    Af,
    /// Registers B and C.
    Bc,
    /// Registers D and E.
    De,
    /// Registers H and L.
    Hl,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn flags_register_masks_reserved_low_nibble() {
        let mut cpu = CpuRegisters::default();
        cpu.set_f(0xFF);
        assert_eq!(cpu.f(), 0xF0);
        cpu.set_flag(flags::ZERO, false);
        assert_eq!(cpu.f(), 0x70);
    }

    #[test]
    fn register_pairs_round_trip_in_hardware_order() {
        let mut cpu = CpuRegisters::default();
        for (pair, value) in [
            (RegisterPair::Af, 0x12F0),
            (RegisterPair::Bc, 0x3456),
            (RegisterPair::De, 0x789A),
            (RegisterPair::Hl, 0xBCDE),
        ] {
            cpu.set_pair(pair, value);
            assert_eq!(cpu.pair(pair), if pair == RegisterPair::Af { value & 0xFFF0 } else { value });
        }
    }

    #[test]
    fn increment_sets_zero_and_half_carry_but_preserves_carry() {
        let mut cpu = CpuRegisters::default();
        cpu.set_flag(flags::CARRY, true);
        assert_eq!(cpu.increment8(0xFF), 0x00);
        assert!(cpu.flag(flags::ZERO));
        assert!(!cpu.flag(flags::SUBTRACT));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(cpu.flag(flags::CARRY));
    }

    #[test]
    fn decrement_sets_half_borrow_and_preserves_carry() {
        let mut cpu = CpuRegisters::default();
        cpu.set_flag(flags::CARRY, true);
        assert_eq!(cpu.decrement8(0x10), 0x0F);
        assert!(!cpu.flag(flags::ZERO));
        assert!(cpu.flag(flags::SUBTRACT));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(cpu.flag(flags::CARRY));
    }

    #[test]
    fn add_a_sets_half_carry_and_carry() {
        let mut cpu = CpuRegisters { a: 0x0F, ..CpuRegisters::default() };
        assert_eq!(cpu.add_a(0x01), 0x10);
        assert_eq!(cpu.a, 0x10);
        assert!(!cpu.flag(flags::ZERO));
        assert!(!cpu.flag(flags::SUBTRACT));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(!cpu.flag(flags::CARRY));

        cpu.a = 0xFF;
        assert_eq!(cpu.add_a(0x01), 0x00);
        assert!(cpu.flag(flags::ZERO));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(cpu.flag(flags::CARRY));
    }

    #[test]
    fn subtract_a_sets_half_borrow_and_borrow() {
        let mut cpu = CpuRegisters { a: 0x10, ..CpuRegisters::default() };
        assert_eq!(cpu.subtract_a(0x01), 0x0F);
        assert!(cpu.flag(flags::SUBTRACT));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(!cpu.flag(flags::CARRY));

        cpu.a = 0;
        assert_eq!(cpu.subtract_a(1), 0xFF);
        assert!(cpu.flag(flags::CARRY));
    }
}
