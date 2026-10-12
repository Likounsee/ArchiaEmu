# Input system

## Current state

Keyboard, controller discovery, hot-plug, remapping and per-system input mapping are not implemented.

## Planned boundary

Represent host devices through a shared, platform-neutral event model, then translate events into the target core's actual controller/peripheral protocol. Keep console-specific button matrices and serial timing inside the relevant core. Configuration should support per-system and per-content overrides with validation and documented defaults.

## Validation

Test remapping conflicts, disconnected devices, hot-plug, invalid configuration, repeat behavior and deterministic input replay. Never claim controller support until a real core consumes events and tests verify the mapping.


## Shared event shape

The core crate defines platform-neutral `InputEvent` state transitions and a small set of semantic `InputControl` values. `EmulationCore::send_input` is the common delivery method; its default implementation explicitly reports unsupported input. Host keyboard/gamepad scan codes must be normalized before they reach a core, and each system module remains responsible for mapping semantic controls to its own hardware. Device discovery, remapping, hotplug, per-system mappings, and a real core consuming these events remain unimplemented.
