# One Game Boy / Game Boy Color emulator

## Decision

The first platform is one unified emulator, identified as `game-boy` and displayed as **Game Boy / Game Boy Color**. Cartridge metadata selects original monochrome Game Boy (DMG) or Game Boy Color (CGB) behavior internally. These are modes of one emulator, not separate modules or application entries. This is a scope decision, not a compatibility claim. External ROMs are not bundled.

## Current implementation

- Header parser validates minimum image length, ROM-size code and header checksum.
- The CGB flag is classified as DMG-only, dual-compatible, CGB-only, or reserved.
- A small LR35902 instruction subset is implemented: NOP, immediate/register loads, 16-bit immediate loads, pair increment/decrement, indirect BC/DE transfers, INC/DEC, ADD A, SUB, XOR, absolute jump and HALT.
- The CPU is connected to a limited memory bus for cartridge ROM, work RAM, echo RAM and high RAM.
- The reset path initializes a post-boot-like register state and starts execution at 0x0100.
- Execution is currently restricted to ROM-only mapper 0 cartridges. Other cartridge types can be inspected but are rejected by the execution path.

This is an early execution milestone, not a playable emulator. The PPU/framebuffer, audio, joypad registers, timers, interrupt arbitration, mapper banking, full opcode coverage, hardware-accurate reset sequence and cycle synchronization are still missing. The descriptor therefore does not advertise full input, video, audio or save-state capabilities.

## Next implementation sequence

1. Expand and validate the LR35902 instruction set, including flag behavior, conditional branches, stack operations and opcode timing.
2. Add exhaustive table-driven CPU tests and instruction-level regressions.
3. Complete address decoding and cartridge-size validation; then add cartridge mapper implementations behind explicit capability checks.
4. Implement interrupt registers/arbitration and the divider/timer.
5. Implement joypad I/O and the PPU scanline pipeline with deterministic framebuffer output.
6. Add CGB-specific registers, VRAM/WRAM banking, palette behavior and double-speed timing inside the same core.
7. Add audio, persistent saves and state serialization after core timing stabilizes.
8. Validate with lawfully obtained test ROMs and documented expected outcomes; do not claim compatibility from compilation alone.

## Correctness rules

- Header parsing is not emulation, and CPU stepping alone is not game compatibility.
- Never panic on malformed cartridge bytes or allocate from untrusted size fields without bounds.
- DMG and CGB remain hardware modes within one Game Boy core. Game Boy Advance is a different system and is out of scope.
- Keep hardware units independently testable and deterministic.
- Do not commit copyrighted test ROMs unless their license expressly permits it; document lawful acquisition and expected results instead.
