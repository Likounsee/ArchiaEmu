#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Minimal terminal host for inspecting registered systems and controlling a session.

use std::error::Error;
use std::io::{self, BufRead, Write};
use std::path::PathBuf;

use archiaemu_core::content::{ContentFormatProbe, ContentLimits};
use archiaemu_core::host_runtime::{HostRuntime, HostRuntimeConfig, SessionId};
use archiaemu_core::runtime::SessionState;
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
    if args.next().is_some() {
        eprintln!("error: expected one content path");
        print_usage();
        std::process::exit(2);
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
    println!("Commands: status, pause, resume, reset, stop, step, quit");
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
            active = wanted.and_then(|n| host.session_ids().into_iter().find(|id| id.get() == n));
            if active.is_none() {
                eprintln!("Unknown or invalid session ID: {}", raw.trim());
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
        if command == "close" || command.starts_with("close ") {
            let target = if let Some(raw) = command.strip_prefix("close ") {
                raw.trim().parse::<u64>().ok().and_then(|n| host.session_ids().into_iter().find(|id| id.get() == n))
            } else { active };
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
            if let Some(id) = active { let _ = host.close_session(id); }
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
                println!("capabilities: input={} video={} audio={} save_states={} persistent_saves={}", caps.input, caps.video_output, caps.audio_output, caps.save_states, caps.persistent_saves);
            }
            "pause" => { host.session_mut(id)?.pause()?; println!("Paused."); }
            "resume" => { host.session_mut(id)?.resume()?; println!("Resumed."); }
            "reset" => { host.session_mut(id)?.reset()?; println!("Reset requested."); }
            "stop" => { host.session_mut(id)?.stop()?; println!("Stopped."); }
            "step" => { let event = host.session_mut(id)?.run_until_event()?; println!("Core event: {event:?}"); }
            "" => {}
            other => eprintln!("Unknown command: {other}. Use status, sessions, switch <id>, open <path>, close [id], pause, resume, reset, stop, step, or quit."),
        }
    }
    Ok(())
}

fn print_usage() {
    println!("ArchiaEmu — minimal terminal host");
    println!("Usage:");
    println!("  archiaemu --list-systems");
    println!("  archiaemu <content-path>");
    println!("  archiaemu --system <system-id> <content-path>");
    println!("  archiaemu --help");
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
}
