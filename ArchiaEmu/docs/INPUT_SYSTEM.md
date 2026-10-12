# Input system

## Current state

Keyboard, controller discovery, hot-plug, remapping and per-system input mapping are not implemented.

## Planned boundary

Represent host devices through a shared, platform-neutral event model, then translate events into the target core's actual controller/peripheral protocol. Keep console-specific button matrices and serial timing inside the relevant core. Configuration should support per-system and per-content overrides with validation and documented defaults.

## Validation

Test remapping conflicts, disconnected devices, hot-plug, invalid configuration, repeat behavior and deterministic input replay. Never claim controller support until a real core consumes events and tests verify the mapping.
\n\n## Shared event shape\n\nThe core crate now defines platform-neutral `InputEvent` state transitions and a small set of semantic `InputControl` values. Host keyboard/gamepad scan codes must be normalized before they reach a core; each system module remains responsible for mapping semantic controls to its own hardware. Device discovery, remapping, hotplug, per-system mappings, and delivery into a running core remain unimplemented.\n