# Input system

## Current state

Keyboard, controller discovery, hot-plug, remapping and per-system input mapping are not implemented.

## Planned boundary

Represent host devices through a shared, platform-neutral event model, then translate events into the target core's actual controller/peripheral protocol. Keep console-specific button matrices and serial timing inside the relevant core. Configuration should support per-system and per-content overrides with validation and documented defaults.

## Validation

Test remapping conflicts, disconnected devices, hot-plug, invalid configuration, repeat behavior and deterministic input replay. Never claim controller support until a real core consumes events and tests verify the mapping.
