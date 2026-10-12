#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! LR35902 register state, arithmetic primitives, and an initial instruction engine.
//!
//! The CPU executes a deliberately small documented opcode subset through a
//! host-provided bus. Full instruction coverage, Game Boy memory mapping,
//! interrupt dispatch, and peripheral cycle synchronization remain incomplete.

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

    /// ANDs an 8-bit value with A and updates Z/N/H/C.
    pub fn and_a(&mut self, value: u8) -> u8 {
        self.a &= value;
        self.set_f(if self.a == 0 { flags::ZERO | flags::HALF_CARRY } else { flags::HALF_CARRY });
        self.a
    }

    /// ORs an 8-bit value with A and updates flags.
    pub fn or_a(&mut self, value: u8) -> u8 {
        self.a |= value;
        self.set_f(if self.a == 0 { flags::ZERO } else { 0 });
        self.a
    }

    /// Compares a value with A without changing A.
    pub fn compare_a(&mut self, value: u8) {
        let a = self.a;
        let (result, borrow) = a.overflowing_sub(value);
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, true);
        self.set_flag(flags::HALF_CARRY, (a & 0x0F) < (value & 0x0F));
        self.set_flag(flags::CARRY, borrow);
    }

    /// Adds an 8-bit value and the carry flag to A.
    pub fn add_a_with_carry(&mut self, value: u8) -> u8 {
        let carry = u8::from(self.flag(flags::CARRY));
        let a = self.a;
        let (partial, carry_one) = a.overflowing_add(value);
        let (result, carry_two) = partial.overflowing_add(carry);
        self.a = result;
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, false);
        self.set_flag(
            flags::HALF_CARRY,
            (a & 0x0F) + (value & 0x0F) + carry > 0x0F,
        );
        self.set_flag(flags::CARRY, carry_one || carry_two);
        result
    }

    /// Subtracts an 8-bit value and the carry flag from A.
    pub fn subtract_a_with_carry(&mut self, value: u8) -> u8 {
        let carry = u8::from(self.flag(flags::CARRY));
        let a = self.a;
        let (partial, borrow_one) = a.overflowing_sub(value);
        let (result, borrow_two) = partial.overflowing_sub(carry);
        self.a = result;
        self.set_flag(flags::ZERO, result == 0);
        self.set_flag(flags::SUBTRACT, true);
        self.set_flag(
            flags::HALF_CARRY,
            (a & 0x0F) < (value & 0x0F) + carry,
        );
        self.set_flag(flags::CARRY, borrow_one || borrow_two);
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

/// Minimal memory interface used by the instruction engine.
///
/// Implementations are responsible for Game Boy address decoding, cartridge
/// banking, I/O registers, and memory access timing. A flat byte array is useful
/// for CPU tests but is not a complete Game Boy memory bus.
pub trait CpuBus {
    /// Reads one byte from the specified 16-bit address.
    fn read8(&mut self, address: u16) -> u8;

    /// Writes one byte to the specified 16-bit address.
    fn write8(&mut self, address: u16, value: u8);
}

/// Result of one successfully executed LR35902 instruction.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct StepResult {
    /// Instruction opcode fetched before execution.
    pub opcode: u8,
    /// Machine cycles consumed (one machine cycle is four clock cycles).
    pub machine_cycles: u8,
}

/// Failure to execute an instruction in the currently implemented subset.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CpuError {
    /// The fetched opcode is not implemented by this execution engine.
    UnsupportedOpcode {
        /// Address from which the opcode was fetched.
        address: u16,
        /// Opcode byte that could not be executed.
        opcode: u8,
    },
}

impl core::fmt::Display for CpuError {
    fn fmt(&self, formatter: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::UnsupportedOpcode { address, opcode } => write!(
                formatter,
                "unsupported LR35902 opcode 0x{opcode:02X} at 0x{address:04X}"
            ),
        }
    }
}

impl std::error::Error for CpuError {}

/// Executable LR35902 CPU state, separated from the system memory bus.
///
/// This is an initial instruction-execution foundation, not yet a complete
/// Game Boy CPU: interrupt dispatch, delayed interrupt enable, STOP behavior,
/// and most opcodes still need implementation and conformance testing.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Cpu {
    /// Visible CPU registers.
    pub registers: CpuRegisters,
    halted: bool,
}

impl Cpu {
    /// Creates a CPU with zeroed registers and execution enabled.
    ///
    /// This state is suitable for controlled tests; a console reset sequence
    /// must initialize hardware-specific register values separately.
    pub const fn new() -> Self {
        Self {
            registers: CpuRegisters {
                a: 0,
                f: 0,
                b: 0,
                c: 0,
                d: 0,
                e: 0,
                h: 0,
                l: 0,
                sp: 0,
                pc: 0,
            },
            halted: false,
        }
    }

    /// Returns whether the processor is currently halted.
    pub const fn is_halted(&self) -> bool {
        self.halted
    }

    /// Leaves the halted state. Interrupt arbitration is not implemented yet.
    pub fn wake(&mut self) {
        self.halted = false;
    }

    /// Fetches and executes one instruction from the provided bus.
    ///
    /// The program counter wraps at 0xFFFF. Unsupported opcodes return an error
    /// after the opcode byte has been fetched; no operands are consumed for an
    /// unsupported instruction.
    pub fn step<B: CpuBus>(&mut self, bus: &mut B) -> Result<StepResult, CpuError> {
        if self.halted {
            return Ok(StepResult {
                opcode: 0x76,
                machine_cycles: 1,
            });
        }

        let address = self.registers.pc;
        let opcode = bus.read8(address);
        self.registers.pc = self.registers.pc.wrapping_add(1);

        let machine_cycles = match opcode {
            0x00 => 1, // NOP
            // LD rr,d16.
            0x01 | 0x11 | 0x21 | 0x31 => {
                let low = self.fetch8(bus);
                let high = self.fetch8(bus);
                let value = u16::from_le_bytes([low, high]);
                match opcode {
                    0x01 => self.registers.set_pair(RegisterPair::Bc, value),
                    0x11 => self.registers.set_pair(RegisterPair::De, value),
                    0x21 => self.registers.set_pair(RegisterPair::Hl, value),
                    _ => self.registers.sp = value,
                }
                3
            }
            // PUSH rr / POP rr.
            0xC5 | 0xD5 | 0xE5 | 0xF5 => {
                self.push_pair(bus, (opcode >> 4) & 0x03);
                4
            }
            0xC1 | 0xD1 | 0xE1 | 0xF1 => {
                self.pop_pair(bus, (opcode >> 4) & 0x03);
                3
            }
            // Immediate ALU operations.
            0xC6 | 0xCE | 0xD6 | 0xDE | 0xE6 | 0xEE | 0xF6 | 0xFE => {
                let value = self.fetch8(bus);
                match opcode {
                    0xC6 => { self.registers.add_a(value); }
                    0xCE => { self.registers.add_a_with_carry(value); }
                    0xD6 => { self.registers.subtract_a(value); }
                    0xDE => { self.registers.subtract_a_with_carry(value); }
                    0xE6 => { self.registers.and_a(value); }
                    0xEE => {
                        self.registers.a ^= value;
                        self.registers.set_f(if self.registers.a == 0 { flags::ZERO } else { 0 });
                    }
                    0xF6 => { self.registers.or_a(value); }
                    _ => self.registers.compare_a(value),
                }
                2
            }
            // CALL a16 / RET.
            0xCD => {
                let low = self.fetch8(bus);
                let high = self.fetch8(bus);
                let target = u16::from_le_bytes([low, high]);
                self.push16(bus, self.registers.pc);
                self.registers.pc = target;
                6
            }
            0xC9 => {
                self.registers.pc = self.pop16(bus);
                4
            }
            // Relative jumps use a signed displacement from the next instruction.
            0x18 | 0x20 | 0x28 | 0x30 | 0x38 => {
                let displacement = self.fetch8(bus) as i8;
                let condition = match opcode {
                    0x18 => true,
                    0x20 => !self.registers.flag(flags::ZERO),
                    0x28 => self.registers.flag(flags::ZERO),
                    0x30 => !self.registers.flag(flags::CARRY),
                    _ => self.registers.flag(flags::CARRY),
                };
                if condition {
                    self.registers.pc =
                        (i32::from(self.registers.pc) + i32::from(displacement)) as u16;
                    3
                } else {
                    2
                }
            }
            // INC rr / DEC rr (flags are not affected).
            0x03 | 0x13 | 0x23 | 0x33 => {
                self.adjust_pair(opcode >> 4, true);
                2
            }
            0x0B | 0x1B | 0x2B | 0x3B => {
                self.adjust_pair(opcode >> 4, false);
                2
            }
            // LD (a16),SP.
            0x08 => {
                let low = self.fetch8(bus);
                let high = self.fetch8(bus);
                let address = u16::from_le_bytes([low, high]);
                let [sp_low, sp_high] = self.registers.sp.to_le_bytes();
                bus.write8(address, sp_low);
                bus.write8(address.wrapping_add(1), sp_high);
                5
            }
            // LD (HL+/-),A and LD A,(HL+/-).
            0x22 | 0x32 => {
                let address = self.registers.pair(RegisterPair::Hl);
                bus.write8(address, self.registers.a);
                let next = if opcode == 0x22 {
                    address.wrapping_add(1)
                } else {
                    address.wrapping_sub(1)
                };
                self.registers.set_pair(RegisterPair::Hl, next);
                2
            }
            0x2A | 0x3A => {
                let address = self.registers.pair(RegisterPair::Hl);
                self.registers.a = bus.read8(address);
                let next = if opcode == 0x2A {
                    address.wrapping_add(1)
                } else {
                    address.wrapping_sub(1)
                };
                self.registers.set_pair(RegisterPair::Hl, next);
                2
            }
            // LD (a16),A / LD A,(a16).
            0xEA | 0xFA => {
                let low = self.fetch8(bus);
                let high = self.fetch8(bus);
                let address = u16::from_le_bytes([low, high]);
                if opcode == 0xEA {
                    bus.write8(address, self.registers.a);
                } else {
                    self.registers.a = bus.read8(address);
                }
                4
            }
            // Indirect A transfers through BC and DE.
            0x02 => {
                bus.write8(self.registers.pair(RegisterPair::Bc), self.registers.a);
                2
            }
            0x12 => {
                bus.write8(self.registers.pair(RegisterPair::De), self.registers.a);
                2
            }
            0x0A => {
                self.registers.a = bus.read8(self.registers.pair(RegisterPair::Bc));
                2
            }
            0x1A => {
                self.registers.a = bus.read8(self.registers.pair(RegisterPair::De));
                2
            }
            0xAF => {
                self.registers.a = 0;
                self.registers.set_f(flags::ZERO);
                1
            }
            0xC3 => {
                let low = self.fetch8(bus);
                let high = self.fetch8(bus);
                self.registers.pc = u16::from_le_bytes([low, high]);
                4
            }
            0x76 => {
                self.halted = true;
                1
            }
            // LD r, r' (including (HL)); HALT is handled above.
            0x40..=0x7F => {
                let destination = (opcode >> 3) & 0x07;
                let source = opcode & 0x07;
                let value = self.read_register(bus, source);
                self.write_register(bus, destination, value);
                if source == 6 || destination == 6 {
                    2
                } else {
                    1
                }
            }
            // ADD A,r; SUB r; XOR A,r.
            0x80..=0x87 => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.add_a(value);
                if opcode & 0x07 == 6 {
                    2
                } else {
                    1
                }
            }
            0x90..=0x97 => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.subtract_a(value);
                if opcode & 0x07 == 6 {
                    2
                } else {
                    1
                }
            }
            0xA0..=0xA7 => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.and_a(value);
                if opcode & 0x07 == 6 { 2 } else { 1 }
            }
            0xA8..=0xAF => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.a ^= value;
                self.registers.set_f(if self.registers.a == 0 {
                    flags::ZERO
                } else {
                    0
                });
                if opcode & 0x07 == 6 { 2 } else { 1 }
            }
            0xB0..=0xB7 => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.or_a(value);
                if opcode & 0x07 == 6 { 2 } else { 1 }
            }
            0xB8..=0xBF => {
                let value = self.read_register(bus, opcode & 0x07);
                self.registers.compare_a(value);
                if opcode & 0x07 == 6 { 2 } else { 1 }
            }
            // INC r / DEC r.
            0x04 | 0x0C | 0x14 | 0x1C | 0x24 | 0x2C | 0x34 | 0x3C => {
                let register = (opcode >> 3) & 0x07;
                let value = self.read_register(bus, register);
                let result = self.registers.increment8(value);
                self.write_register(bus, register, result);
                if register == 6 {
                    3
                } else {
                    1
                }
            }
            0x05 | 0x0D | 0x15 | 0x1D | 0x25 | 0x2D | 0x35 | 0x3D => {
                let register = (opcode >> 3) & 0x07;
                let value = self.read_register(bus, register);
                let result = self.registers.decrement8(value);
                self.write_register(bus, register, result);
                if register == 6 {
                    3
                } else {
                    1
                }
            }
            // LD r,d8.
            0x06 | 0x0E | 0x16 | 0x1E | 0x26 | 0x2E | 0x36 | 0x3E => {
                let register = (opcode >> 3) & 0x07;
                let value = self.fetch8(bus);
                self.write_register(bus, register, value);
                if register == 6 {
                    3
                } else {
                    2
                }
            }
            _ => return Err(CpuError::UnsupportedOpcode { address, opcode }),
        };

        Ok(StepResult {
            opcode,
            machine_cycles,
        })
    }

    fn push_pair<B: CpuBus>(&mut self, bus: &mut B, pair: u8) {
        let value = match pair {
            0 => self.registers.pair(RegisterPair::Bc),
            1 => self.registers.pair(RegisterPair::De),
            2 => self.registers.pair(RegisterPair::Hl),
            _ => self.registers.pair(RegisterPair::Af),
        };
        self.push16(bus, value);
    }

    fn pop_pair<B: CpuBus>(&mut self, bus: &mut B, pair: u8) {
        let value = self.pop16(bus);
        match pair {
            0 => self.registers.set_pair(RegisterPair::Bc, value),
            1 => self.registers.set_pair(RegisterPair::De, value),
            2 => self.registers.set_pair(RegisterPair::Hl, value),
            _ => self.registers.set_pair(RegisterPair::Af, value),
        }
    }

    fn push16<B: CpuBus>(&mut self, bus: &mut B, value: u16) {
        let [high, low] = value.to_be_bytes();
        self.registers.sp = self.registers.sp.wrapping_sub(1);
        bus.write8(self.registers.sp, high);
        self.registers.sp = self.registers.sp.wrapping_sub(1);
        bus.write8(self.registers.sp, low);
    }

    fn pop16<B: CpuBus>(&mut self, bus: &mut B) -> u16 {
        let low = bus.read8(self.registers.sp);
        self.registers.sp = self.registers.sp.wrapping_add(1);
        let high = bus.read8(self.registers.sp);
        self.registers.sp = self.registers.sp.wrapping_add(1);
        u16::from_le_bytes([low, high])
    }

    fn adjust_pair(&mut self, pair: u8, increment: bool) {
        let value = match pair {
            0 => self.registers.pair(RegisterPair::Bc),
            1 => self.registers.pair(RegisterPair::De),
            2 => self.registers.pair(RegisterPair::Hl),
            _ => self.registers.sp,
        };
        let adjusted = if increment {
            value.wrapping_add(1)
        } else {
            value.wrapping_sub(1)
        };
        match pair {
            0 => self.registers.set_pair(RegisterPair::Bc, adjusted),
            1 => self.registers.set_pair(RegisterPair::De, adjusted),
            2 => self.registers.set_pair(RegisterPair::Hl, adjusted),
            _ => self.registers.sp = adjusted,
        }
    }

    fn read_register<B: CpuBus>(&self, bus: &mut B, register: u8) -> u8 {
        match register {
            0 => self.registers.b,
            1 => self.registers.c,
            2 => self.registers.d,
            3 => self.registers.e,
            4 => self.registers.h,
            5 => self.registers.l,
            6 => bus.read8(self.registers.pair(RegisterPair::Hl)),
            _ => self.registers.a,
        }
    }

    fn write_register<B: CpuBus>(&mut self, bus: &mut B, register: u8, value: u8) {
        match register {
            0 => self.registers.b = value,
            1 => self.registers.c = value,
            2 => self.registers.d = value,
            3 => self.registers.e = value,
            4 => self.registers.h = value,
            5 => self.registers.l = value,
            6 => bus.write8(self.registers.pair(RegisterPair::Hl), value),
            _ => self.registers.a = value,
        }
    }

    fn fetch8<B: CpuBus>(&mut self, bus: &mut B) -> u8 {
        let value = bus.read8(self.registers.pc);
        self.registers.pc = self.registers.pc.wrapping_add(1);
        value
    }
}

#[cfg(test)]
mod execution_tests {
    use super::*;

    struct TestBus([u8; 0x10000]);

    impl TestBus {
        fn with_program(program: &[u8]) -> Self {
            let mut bus = Self([0; 0x10000]);
            bus.0[..program.len()].copy_from_slice(program);
            bus
        }
    }

    impl CpuBus for TestBus {
        fn read8(&mut self, address: u16) -> u8 {
            self.0[usize::from(address)]
        }

        fn write8(&mut self, address: u16, value: u8) {
            self.0[usize::from(address)] = value;
        }
    }

    #[test]
    fn immediate_load_advances_pc_and_reports_machine_cycles() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[0x3E, 0x42, 0x00]);

        assert_eq!(
            cpu.step(&mut bus),
            Ok(StepResult {
                opcode: 0x3E,
                machine_cycles: 2
            })
        );
        assert_eq!(cpu.registers.a, 0x42);
        assert_eq!(cpu.registers.pc, 2);
        assert_eq!(
            cpu.step(&mut bus),
            Ok(StepResult {
                opcode: 0x00,
                machine_cycles: 1
            })
        );
        assert_eq!(cpu.registers.pc, 3);
    }

    #[test]
    fn stores_immediate_value_through_hl() {
        let mut cpu = Cpu::new();
        cpu.registers.set_pair(RegisterPair::Hl, 0xC123);
        let mut bus = TestBus::with_program(&[0x36, 0xA5]);

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(bus.0[0xC123], 0xA5);
        assert_eq!(cpu.registers.pc, 2);
    }

    #[test]
    fn xor_a_sets_zero_and_clears_other_flags() {
        let mut cpu = Cpu::new();
        cpu.registers.a = 0xFF;
        cpu.registers.set_f(0xF0);
        let mut bus = TestBus::with_program(&[0xAF]);

        cpu.step(&mut bus).unwrap();
        assert_eq!(cpu.registers.a, 0);
        assert_eq!(cpu.registers.f(), flags::ZERO);
    }

    #[test]
    fn absolute_jump_reads_little_endian_target() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[0xC3, 0x34, 0x12]);

        let result = cpu.step(&mut bus).unwrap();
        assert_eq!(result.machine_cycles, 4);
        assert_eq!(cpu.registers.pc, 0x1234);
    }

    #[test]
    fn halt_is_stable_until_explicitly_woken() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[0x76, 0x00]);

        assert_eq!(cpu.step(&mut bus).unwrap().opcode, 0x76);
        assert!(cpu.is_halted());
        assert_eq!(cpu.registers.pc, 1);
        assert_eq!(cpu.step(&mut bus).unwrap().opcode, 0x76);
        assert_eq!(cpu.registers.pc, 1);

        cpu.wake();
        assert_eq!(cpu.step(&mut bus).unwrap().opcode, 0x00);
        assert!(!cpu.is_halted());
    }

    #[test]
    fn hl_auto_increment_and_absolute_memory_transfers_work() {
        let mut cpu = Cpu::new();
        cpu.registers.set_pair(RegisterPair::Hl, 0xC100);
        cpu.registers.a = 0x42;
        let mut bus = TestBus::with_program(&[
            0x22,             // LD (HL+),A
            0x3A,             // LD A,(HL-)
            0xEA, 0x01, 0xC1, // LD (0xC101),A
            0xFA, 0x01, 0xC1, // LD A,(0xC101)
        ]);
        bus.0[0xC101] = 0xFF;

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(bus.0[0xC100], 0x42);
        assert_eq!(cpu.registers.pair(RegisterPair::Hl), 0xC101);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(cpu.registers.a, 0xFF);
        assert_eq!(cpu.registers.pair(RegisterPair::Hl), 0xC100);
        cpu.registers.a = 0xA5;
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 4);
        assert_eq!(bus.0[0xC101], 0xA5);
        cpu.registers.a = 0;
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 4);
        assert_eq!(cpu.registers.a, 0xA5);
    }

    #[test]
    fn call_return_and_stack_pairs_round_trip() {
        let mut cpu = Cpu::new();
        cpu.registers.sp = 0xC100;
        cpu.registers.set_pair(RegisterPair::Bc, 0xBEEF);
        let mut bus = TestBus::with_program(&[
            0xC5, // PUSH BC
            0x01, 0x00, 0x00, // LD BC,0
            0xC1, // POP BC
            0xCD, 0x09, 0x00, // CALL 0x0009
            0x00, // return address 0x0008, then NOP
            0xC9, // RET
        ]);

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 4);
        assert_eq!(cpu.registers.sp, 0xC0FE);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(cpu.registers.pair(RegisterPair::Bc), 0);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(cpu.registers.pair(RegisterPair::Bc), 0xBEEF);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 6);
        assert_eq!(cpu.registers.pc, 0x0009);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 4);
        assert_eq!(cpu.registers.pc, 0x0008);
    }

    #[test]
    fn relative_jumps_apply_signed_offsets_and_test_flags() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[
            0x18, 0x02, // JR +2: skip the next two bytes
            0x06, 0x11, 0x20, 0xFC, // JR NZ,-4 (not taken because Z is set below)
        ]);

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(cpu.registers.pc, 4);
        cpu.registers.set_flag(flags::ZERO, true);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(cpu.registers.pc, 6);
    }

    #[test]
    fn sixteen_bit_load_and_pair_adjustment_execute() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[
            0x01, 0xFF, 0x12, // LD BC,0x12FF
            0x03, // INC BC
            0x11, 0x34, 0x12, // LD DE,0x1234
            0x12, // LD (DE),A
            0x0A, // LD A,(BC)
        ]);
        cpu.registers.a = 0x5A;

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(cpu.registers.pair(RegisterPair::Bc), 0x12FF);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(cpu.registers.pair(RegisterPair::Bc), 0x1300);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(cpu.registers.pair(RegisterPair::De), 0x1234);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(bus.0[0x1234], 0x5A);
        bus.0[0x1300] = 0xC7;
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(cpu.registers.a, 0xC7);
    }

    #[test]
    fn register_to_register_load_and_alu_group_execute() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[
            0x06, 0x0F, // LD B,0x0F
            0x78, // LD A,B
            0x80, // ADD A,B
            0xA8, // XOR A,B
        ]);

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 2);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 1);
        assert_eq!(cpu.registers.a, 0x0F);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 1);
        assert_eq!(cpu.registers.a, 0x1E);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 1);
        assert_eq!(cpu.registers.a, 0x11);
        assert_eq!(cpu.registers.pc, 5);
    }

    #[test]
    fn memory_register_load_and_increment_use_hl_bus_address() {
        let mut cpu = Cpu::new();
        cpu.registers.set_pair(RegisterPair::Hl, 0xC123);
        let mut bus = TestBus::with_program(&[0x36, 0x0F, 0x34, 0x35]);

        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(bus.0[0xC123], 0x0F);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(bus.0[0xC123], 0x10);
        assert_eq!(cpu.step(&mut bus).unwrap().machine_cycles, 3);
        assert_eq!(bus.0[0xC123], 0x0F);
    }

    #[test]
    fn unsupported_opcode_is_reported_with_fetch_address() {
        let mut cpu = Cpu::new();
        let mut bus = TestBus::with_program(&[0xD3]);

        assert_eq!(
            cpu.step(&mut bus),
            Err(CpuError::UnsupportedOpcode {
                address: 0,
                opcode: 0xD3
            })
        );
        assert_eq!(cpu.registers.pc, 1);
    }
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
            assert_eq!(
                cpu.pair(pair),
                if pair == RegisterPair::Af {
                    value & 0xFFF0
                } else {
                    value
                }
            );
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
        let mut cpu = CpuRegisters {
            a: 0x0F,
            ..CpuRegisters::default()
        };
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
        let mut cpu = CpuRegisters {
            a: 0x10,
            ..CpuRegisters::default()
        };
        assert_eq!(cpu.subtract_a(0x01), 0x0F);
        assert!(cpu.flag(flags::SUBTRACT));
        assert!(cpu.flag(flags::HALF_CARRY));
        assert!(!cpu.flag(flags::CARRY));

        cpu.a = 0;
        assert_eq!(cpu.subtract_a(1), 0xFF);
        assert!(cpu.flag(flags::CARRY));
    }
}
