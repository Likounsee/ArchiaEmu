# ArchiaEmu Desktop — UI plan

## Product goal

Build a dependable desktop application for managing supported content and sessions before investing in additional console modules. The interface must never imply that a core is playable merely because it is registered or a file was imported.

## Layout

```text
┌──────────────────────┬─────────────────────────────────────────────┐
│ ◈ ArchiaEmu          │ Current page                  N sessions    │
│ EMULATION STUDIO     ├─────────────────────────────────────────────┤
│                      │                                             │
│ WORKSPACE            │ Page content                                │
│  Overview            │                                             │
│  Game library        │  Primary action + useful state              │
│  Sessions            │  Search, details, lifecycle controls         │
│  Game                │                                             │
│  Systems             │                                             │
│  Settings            │                                             │
│                      ├─────────────────────────────────────────────┤
│ Status / version     │ ● Recoverable status or error message       │
└──────────────────────┴─────────────────────────────────────────────┘
```

The layout uses a stable left navigation rail, a clear page header, a central workspace and a small status area. It should remain readable at the minimum supported window size and scale with the user's display setting.

## Pages

### Overview

- Show useful counts for library entries, active sessions and registered systems.
- Provide obvious actions to import content and open the library.
- Show recent or selected content only when it exists.
- Use a concise empty state when the user has not imported anything.

### Game library

- Search by title or path; filter by detected/supported format and sort predictably.
- Show title, file size, path and selection state.
- Keep Import, Select, Launch and Remove actions near each item.
- Distinguish “in the library” from “validated for launch” and from “known compatible”.
- Preserve the library between restarts and explain missing/unavailable files.

### Sessions

- List every session with content title, system, lifecycle state and available capabilities.
- Make switching, pause/resume, reset, stop and close explicit.
- Confirm or safely handle bulk close; never leave a stale active-session selection.
- Explain unsupported actions instead of silently doing nothing.

### Game

- Give the content/media surface the most space.
- Show lifecycle controls and input hints nearby.
- Display an explicit no-video/no-audio state when the selected core does not provide media.
- Release held input on focus loss, leaving the page or closing the session.
- Do not generate fake frames or claim that launch equals compatibility.

### Systems

- List registered systems and declared capabilities.
- Separate “registered”, “installed”, “can load this format” and “tested playable” as different concepts.
- Avoid console-specific configuration until that module is part of the roadmap.

### Settings

- Keep host preferences separate from emulated hardware settings.
- Start with theme and UI scale; add audio/video devices, shortcuts and library paths only when implemented.
- Validate values, preserve user settings and provide a clear reset-to-default action.

## Visual and interaction principles

- Keep the existing restrained dark/light theme, spacing and rounded controls consistent.
- Prefer clear labels over icon-only controls.
- Make primary actions visually distinct without crowding every card with buttons.
- Every empty state should provide a next step.
- Every error should say what failed and, where possible, how to recover.
- Disabled or unavailable features must explain why.
- Keyboard navigation and focus handling are part of correctness, not optional polish.

## Implementation order

1. Split page rendering into modules while keeping the current behavior stable.
2. Add explicit UI/view states and finish the controller boundary so page code does not inspect mutable runtime internals.
3. Add tests for library/session/preferences state transitions and user-facing failures.
4. Persist and recover the library reliably.
5. Complete session diagnostics and settings behavior.
6. Integrate testable host media adapters where supported; keep the UI honest when media is absent.
7. Run formatting, workspace tests, clippy, docs and desktop CI on Linux, Windows and macOS.
8. Only after the application usability criteria pass, resume deeper module management and real console implementation.

## Acceptance criteria

- A new user can open the application and understand what to do next.
- Import, search, select, launch and failure paths have clear feedback.
- Session switching and closing never leave stale UI state or held inputs.
- Preferences survive restart and malformed config does not crash the app.
- Unsupported capabilities are accurately described.
- The current commit passes the configured Rust quality gates and desktop CI.
