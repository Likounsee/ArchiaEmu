#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Minimal terminal host for inspecting registered systems and controlling a session.

use std::error::Error;
use std::io::{self, BufRead, Write};
use std::path::PathBuf;

use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeConfig, SessionId};
use archiaemu_core::save_state_store::SaveStateStore;
use archiaemu_core::core_io::{InputControl, InputEvent};
use archiaemu_core::CoreRegistry;
use archiaemu_game_boy::{register_game_boy, GameBoyCartridgeProbe};

const MAX_CONTENT_BYTES: usize = 32 * 1024 * 1024;

/// Starts the command-line host.
fn main() -> Result<(), Box<dyn Error>> {
    let mut args = std::env::args_os().skip(1);
    let Some(first) = args.next() else {
        print_usage();
        return Ok(());
    };

    let mut registry = CoreRegistry::new();
    register_game_boy(&mut registry)?;
    if first == "--list-systems" {
        list_systems(&registry);
        return Ok(());
    }
    if first == "--help" || first == "-h" {
        print_usage();
        return Ok(());
    }
    let (path, selected_system) = if first == "--system" {
        let selected = args
            .next()
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "missing system ID"))?;
        let content = args
            .next()
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "missing content path"))?;
        if args.next().is_some() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "expected --system <system-id> <content-path>",
            )
            .into());
        }
        (
            PathBuf::from(content),
            Some(selected.to_string_lossy().into_owned()),
        )
    } else {
        if args.next().is_some() {
            eprintln!("error: expected one content path");
            print_usage();
            std::process::exit(2);
        }
        (PathBuf::from(first), None)
    };

    let probe = GameBoyCartridgeProbe;
    if let Some(selected) = selected_system.as_deref() {
        validate_system_selection(&registry, selected, probe.format_id())
            .map_err(|message| io::Error::new(io::ErrorKind::InvalidInput, message))?;
    }
    let probes: [&dyn ContentFormatProbe; 1] = [&probe];
    let mut host = HostRuntime::new(HostRuntimeConfig::default())?;
    let session_id = host.launch_content_path(
        &path,
        ContentLimits::new(MAX_CONTENT_BYTES),
        &probes,
        &registry,
    )?;
    println!("Content loaded into host session {}.", session_id.get());
    println!("This core is an early implementation, not yet a playable emulator.");
    println!("Commands: status, sessions, switch <id>, open <path>, close [id], input <control> <press|release>, release-inputs, media, save-state <path>, load-state <path>, pause, resume, reset, stop, step, quit");
    command_loop(&mut host, session_id, &probe, &registry)
}

fn list_systems(registry: &CoreRegistry) {
    for id in registry.system_ids() {
        if let Some(descriptor) = registry.registered_descriptor(id) {
            let capabilities = descriptor.capabilities;
            println!("{} — {}", id.as_str(), descriptor.display_name);
            println!(
                "  content={} input={} video={} audio={} save_states={} persistent_saves={}",
                capabilities.content_loading,
                capabilities.input,
                capabilities.video_output,
                capabilities.audio_output,
                capabilities.save_states,
                capabilities.persistent_saves
            );
        } else {
            println!("{} — metadata unavailable", id.as_str());
        }
    }
}

fn command_loop(
    host: &mut HostRuntime,
    initial_id: SessionId,
    probe: &dyn ContentFormatProbe,
    registry: &CoreRegistry,
) -> Result<(), Box<dyn Error>> {
    let stdin = io::stdin();
    let mut lines = stdin.lock().lines();
    let mut active = Some(initial_id);
    loop {
        print!("archiaemu> ");
        io::stdout().flush()?;
        let Some(line) = lines.next() else {
            if let Some(id) = active {
                let _ = host.close_session(id);
            }
            break;
        };
        let line = line?;
        let command = line.trim();
        if command == "sessions" {
            for id in host.session_ids() {
                let session = host.session(id)?;
                let marker = if active == Some(id) { "*" } else { " " };
                println!(
                    "{marker} {} system={} state={:?}",
                    id.get(),
                    session.descriptor().system_id.as_str(),
                    session.state()
                );
            }
            continue;
        }
        if let Some(raw) = command.strip_prefix("switch ") {
            let wanted = raw.trim().parse::<u64>().ok();
            match wanted.and_then(|n| host.session_ids().into_iter().find(|id| id.get() == n)) {
                Some(id) => active = Some(id),
                None => eprintln!("Unknown or invalid session ID: {}", raw.trim()),
            }
            continue;
        }
        if let Some(path) = command.strip_prefix("open ") {
            match host.launch_content_path(
                PathBuf::from(path.trim()),
                ContentLimits::new(MAX_CONTENT_BYTES),
                &[probe],
                registry,
            ) {
                Ok(id) => {
                    active = Some(id);
                    println!("Opened session {}.", id.get());
                }
                Err(error) => eprintln!("Could not open content: {error}"),
            }
            continue;
        }
        if let Some(raw) = command.strip_prefix("input ") {
            match apply_input_command(host, active, raw) {
                Ok(message) => println!("{message}"),
                Err(error) => eprintln!("Input failed: {error}"),
            }
            continue;
        }
        if command == "release-inputs" {
            match release_session_inputs(host, active) {
                Ok(()) => println!("All held inputs released."),
                Err(error) => eprintln!("Could not release inputs: {error}"),
            }
            continue;
        }
        if command == "media" {
            match inspect_media(host, active) {
                Ok(report) => println!("{report}"),
                Err(error) => eprintln!("Media retrieval failed: {error}"),
            }
            continue;
        }
        if let Some(path) = command.strip_prefix("save-state ") {
            match save_state_to_path(host, active, path.trim()) {
                Ok(()) => println!("Save state written."),
                Err(error) => eprintln!("Could not save state: {error}"),
            }
            continue;
        }
        if let Some(path) = command.strip_prefix("load-state ") {
            match load_state_from_path(host, active, path.trim()) {
                Ok(()) => println!("Save state restored."),
                Err(error) => eprintln!("Could not load state: {error}"),
            }
            continue;
        }
        if command == "close" || command.starts_with("close ") {
            let target = if let Some(raw) = command.strip_prefix("close ") {
                raw.trim()
                    .parse::<u64>()
                    .ok()
                    .and_then(|n| host.session_ids().into_iter().find(|id| id.get() == n))
            } else {
                active
            };
            match target {
                Some(id) => {
                    host.close_session(id)?;
                    if active == Some(id) {
                        active = host.session_ids().into_iter().next();
                    }
                    println!("Closed session {}.", id.get());
                }
                None => eprintln!("No matching session to close."),
            }
            continue;
        }
        if command == "quit" || command == "exit" {
            if let Some(id) = active {
                let _ = host.close_session(id);
            }
            break;
        }
        let Some(id) = active else {
            eprintln!("No active session. Use open <content-path> first.");
            continue;
        };
        match command {
            "status" => {
                let session = host.session(id)?;
                let caps = session.descriptor().capabilities;
                println!("session={} state={:?}", id.get(), session.state());
                println!(
                    "capabilities: input={} video={} audio={} save_states={} persistent_saves={}",
                    caps.input,
                    caps.video_output,
                    caps.audio_output,
                    caps.save_states,
                    caps.persistent_saves
                );
            }
            "pause" | "resume" | "reset" | "stop" | "step" => {
                match apply_session_command(host, id, command) {
                    Ok(message) => println!("{message}"),
                    Err(error) => eprintln!("Command failed: {error}"),
                }
            }
            "" => {}
            other => eprintln!(
                "Unknown command: {other}. Use status, sessions, switch <id>, open <path>, close [id], input <control> <press|release>, release-inputs, media, save-state <path>, load-state <path>, pause, resume, reset, stop, step, or quit."
            ),
        }
    }
    Ok(())
}

fn parse_input_control(raw: &str) -> Option<InputControl> {
    match raw {
        "up" => Some(InputControl::Up),
        "down" => Some(InputControl::Down),
        "left" => Some(InputControl::Left),
        "right" => Some(InputControl::Right),
        "action1" => Some(InputControl::Action1),
        "action2" => Some(InputControl::Action2),
        "action3" => Some(InputControl::Action3),
        "action4" => Some(InputControl::Action4),
        "start" => Some(InputControl::Start),
        "select" => Some(InputControl::Select),
        _ => None,
    }
}

fn apply_input_command(
    host: &mut HostRuntime,
    active: Option<SessionId>,
    raw: &str,
) -> Result<String, String> {
    let mut parts = raw.split_whitespace();
    let control_name = parts
        .next()
        .ok_or_else(|| "expected input <control> <press|release>".to_owned())?;
    let action = parts.next().ok_or_else(|| "expected input <control> <press|release>".to_owned())?;
    if parts.next().is_some() {
        return Err("expected exactly two arguments: <control> <press|release>".to_owned());
    }
    let control = parse_input_control(control_name)
        .ok_or_else(|| format!("unknown control '{control_name}'"))?;
    let pressed = match action {
        "press" => true,
        "release" => false,
        _ => return Err(format!("unknown input action '{action}'; use press or release")),
    };
    let id = active.ok_or_else(|| "no active session".to_owned())?;
    host.session_mut(id)
        .map_err(|error| error.to_string())?
        .send_input(InputEvent { control, pressed })
        .map_err(|error| error.to_string())?;
    Ok(format!("{} {}.", control_name, action))
}

fn release_session_inputs(
    host: &mut HostRuntime,
    active: Option<SessionId>,
) -> Result<(), String> {
    let id = active.ok_or_else(|| "no active session".to_owned())?;
    host.session_mut(id)
        .map_err(|error| error.to_string())?
        .release_all_inputs()
        .map_err(|error| error.to_string())
}

fn inspect_media(
    host: &mut HostRuntime,
    active: Option<SessionId>,
) -> Result<String, String> {
    let id = active.ok_or_else(|| "no active session".to_owned())?;
    let session = host.session_mut(id).map_err(|error| error.to_string())?;
    let capabilities = session.descriptor().capabilities;
    let mut report = Vec::with_capacity(2);

    if capabilities.video_output {
        match session.take_video_frame().map_err(|error| error.to_string())? {
            Some(frame) => report.push(format!(
                "video: {}x{} {:?}, stride={} bytes, payload={} bytes",
                frame.width(),
                frame.height(),
                frame.format(),
                frame.row_stride_bytes(),
                frame.pixels().len()
            )),
            None => report.push("video: no frame ready".to_owned()),
        }
    } else {
        report.push("video: unsupported".to_owned());
    }

    if capabilities.audio_output {
        match session.take_audio_buffer().map_err(|error| error.to_string())? {
            Some(buffer) => report.push(format!(
                "audio: {} Hz, {} channel(s), {} sample frame(s)",
                buffer.sample_rate(), buffer.channels(), buffer.frame_count()
            )),
            None => report.push("audio: no buffer ready".to_owned()),
        }
    } else {
        report.push("audio: unsupported".to_owned());
    }

    Ok(report.join("; "))
}

fn save_state_to_path(
    host: &mut HostRuntime,
    active: Option<SessionId>,
    path: &str,
) -> Result<(), String> {
    let id = active.ok_or_else(|| "no active session".to_owned())?;
    let session = host.session_mut(id).map_err(|error| error.to_string())?;
    if !session.descriptor().capabilities.save_states {
        return Err("active core does not support emulator save states".to_owned());
    }
    let envelope = session
        .save_state_envelope()
        .map_err(|error| error.to_string())?;
    SaveStateStore::new(path)
        .save(&envelope)
        .map_err(|error| error.to_string())
}

fn load_state_from_path(
    host: &mut HostRuntime,
    active: Option<SessionId>,
    path: &str,
) -> Result<(), String> {
    let id = active.ok_or_else(|| "no active session".to_owned())?;
    let session = host.session(id).map_err(|error| error.to_string())?;
    if !session.descriptor().capabilities.save_states {
        return Err("active core does not support emulator save states".to_owned());
    }
    let envelope = SaveStateStore::new(path)
        .load()
        .map_err(|error| error.to_string())?;
    host.session_mut(id)
        .map_err(|error| error.to_string())?
        .load_state_envelope(&envelope)
        .map_err(|error| error.to_string())
}

fn apply_session_command(
    host: &mut HostRuntime,
    id: SessionId,
    command: &str,
) -> Result<String, String> {
    let session = host.session_mut(id).map_err(|error| error.to_string())?;
    match command {
        "pause" => {
            session.pause().map_err(|error| error.to_string())?;
            Ok("Paused.".to_owned())
        }
        "resume" => {
            session.resume().map_err(|error| error.to_string())?;
            Ok("Resumed.".to_owned())
        }
        "reset" => {
            session.reset().map_err(|error| error.to_string())?;
            Ok("Reset requested.".to_owned())
        }
        "stop" => {
            session.stop().map_err(|error| error.to_string())?;
            Ok("Stopped.".to_owned())
        }
        "step" => {
            let event = session
                .run_until_event()
                .map_err(|error| error.to_string())?;
            Ok(format!("Core event: {event:?}"))
        }
        _ => Err(format!("Unsupported session command: {command}")),
    }
}

fn print_usage() {
    println!("ArchiaEmu — minimal terminal host");
    println!("Usage:");
    println!("  archiaemu --list-systems");
    println!("  archiaemu <content-path>");
    println!("  archiaemu --system <system-id> <content-path>");
    println!("  archiaemu --help");
    println!("Interactive input: input <up|down|left|right|action1|action2|action3|action4|start|select> <press|release>");
    println!("                  release-inputs releases all held controls.");
    println!("                  media polls one video frame/audio buffer and reports metadata.");
}

fn validate_system_selection(
    registry: &CoreRegistry,
    selected: &str,
    probe_format: &str,
) -> Result<(), String> {
    if !registry.system_ids().any(|id| id.as_str() == selected) {
        return Err(format!("unknown system ID: {selected}"));
    }
    if selected != probe_format {
        return Err(format!(
            "system {selected} has no content probe registered by this frontend"
        ));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn registry() -> CoreRegistry {
        let mut registry = CoreRegistry::new();
        register_game_boy(&mut registry).unwrap();
        registry
    }

    #[test]
    fn explicit_system_selection_accepts_a_registered_matching_probe() {
        let registry = registry();
        assert!(validate_system_selection(&registry, "game-boy", "game-boy").is_ok());
    }

    #[test]
    fn explicit_system_selection_rejects_unknown_systems_and_probe_mismatches() {
        let registry = registry();
        assert!(validate_system_selection(&registry, "unknown-system", "unknown-system").is_err());
        assert!(validate_system_selection(&registry, "game-boy", "another-format").is_err());
    }

    #[test]
    fn input_control_parser_accepts_every_semantic_control_and_rejects_unknown_names() {
        let controls = [
            ("up", InputControl::Up),
            ("down", InputControl::Down),
            ("left", InputControl::Left),
            ("right", InputControl::Right),
            ("action1", InputControl::Action1),
            ("action2", InputControl::Action2),
            ("action3", InputControl::Action3),
            ("action4", InputControl::Action4),
            ("start", InputControl::Start),
            ("select", InputControl::Select),
        ];
        for (name, expected) in controls {
            assert_eq!(parse_input_control(name), Some(expected));
        }
        assert_eq!(parse_input_control("spacebar"), None);
    }

    #[test]
    fn media_inspection_reports_unsupported_outputs_without_polling_the_core() {
        let mut host = HostRuntime::new(HostRuntimeConfig::default()).unwrap();
        let id = host
            .create_session(Box::new(archiaemu_game_boy::GameBoyCore::new()))
            .unwrap();

        assert_eq!(
            inspect_media(&mut host, Some(id)).unwrap(),
            "video: unsupported; audio: unsupported"
        );
    }

    #[test]
    fn media_inspection_requires_an_active_session() {
        let mut host = HostRuntime::new(HostRuntimeConfig::default()).unwrap();
        assert_eq!(inspect_media(&mut host, None).unwrap_err(), "no active session");
    }

    #[test]
    fn save_state_commands_report_unsupported_capability_before_file_access() {
        let mut host = HostRuntime::new(HostRuntimeConfig::default()).unwrap();
        let id = host
            .create_session(Box::new(archiaemu_game_boy::GameBoyCore::new()))
            .unwrap();
        let path = std::env::temp_dir().join(format!(
            "archiaemu-unsupported-state-{}.bin",
            std::process::id()
        ));
        let path_text = path.to_string_lossy();

        let save_error = save_state_to_path(&mut host, Some(id), &path_text).unwrap_err();
        let load_error = load_state_from_path(&mut host, Some(id), &path_text).unwrap_err();

        assert!(save_error.contains("does not support emulator save states"));
        assert!(load_error.contains("does not support emulator save states"));
        assert!(!path.exists());
    }
}
