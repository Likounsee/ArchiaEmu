# First target: one Game Boy / Game Boy Color emulator

## Decision

The first planned platform is one unified Game Boy emulator, identified as `game-boy` and displayed to users as **Game Boy / Game Boy Color**. It will use cartridge metadata to choose original monochrome Game Boy (DMG) or Game Boy Color (CGB) hardware behavior internally. These are modes of one emulator, not separate modules or application entries. This is a scope decision, not a support claim. Only test content whose license and redistribution terms are checked may be committed; external ROMs are not bundled.

## Implementation sequence

1. **Cartridge boundary (current):** safely parse header length, ROM-size code, and header checksum; preserve raw metadata without implying mapper support.
2. **Cartridge model:** validate image length against ROM-size code, classify DMG/CGB compatibility flags, and define explicit mapper capability for both hardware modes.
3. **Memory bus:** address decoding and tests for ROM-only cartridges, VRAM/WRAM, echo RAM, OAM, unusable region, and I/O boundaries.
4. **CPU:** implement LR35902 registers, flags, instruction decode/execute, cycle accounting, interrupts, HALT/STOP behavior and exhaustive opcode tests.
5. **Timers and interrupts:** deterministic clock-driven timer, divider and interrupt controller.
6. **PPU and input:** scanline timing, framebuffer, joypad matrix and timing regression tests.
7. **Audio and persistence:** only after CPU/bus/PPU contracts stabilize.
8. **Compatibility gates:** documented public test ROMs, known hashes where legally appropriate, expected outcomes, and regression reports.

## Current CPU foundation\n\n`src/cpu.rs` contains only deterministic register/flag primitives. It is not an instruction interpreter and is not connected to the module's run loop yet. Its tests cover reserved flag bits, register-pair ordering, Z/N/H/C behavior for basic arithmetic, and carry preservation for INC/DEC. The core must continue to report execution as unsupported until a real instruction step and bus are integrated.\n\n## Correctness rules

- Header parsing is not emulation, and a passing parser test is not a compatibility result.
- Never panic on malformed cartridge bytes or allocate from untrusted size fields without bounds.
- Implement DMG and CGB hardware behavior as modes inside the same Game Boy core. Game Boy Advance is a different system and is out of scope.
- Keep each hardware unit independently testable and deterministic.
- Use external test ROMs only under their respective licenses; document how to obtain them rather than bundling them by default.
