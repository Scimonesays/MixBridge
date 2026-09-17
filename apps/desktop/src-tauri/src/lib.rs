#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use once_cell::sync::Lazy;
use serde::Serialize;
use std::io::{BufRead, BufReader, Write};
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;
use std::time::Duration;
use tauri::Manager;

static ENGINE: Lazy<Mutex<EngineClient>> = Lazy::new(|| Mutex::new(EngineClient::default()));

#[derive(Default)]
struct EngineClient {
  child: Option<Child>,
}

#[derive(Serialize)]
struct StatusDto {
  state: String,
  raw: String,
}

#[derive(Serialize)]
struct MeterDto {
  peak: f32,
  rms: f32,
  clip: bool,
}

#[derive(Serialize)]
struct IdDto {
  id: u32,
}

fn pipe_command(cmd: &str) -> Result<String, String> {
  #[cfg(windows)]
  {
    use std::fs::OpenOptions;
    let mut pipe = OpenOptions::new()
      .read(true)
      .write(true)
      .open(r"\\.\pipe\mixbridge-engine")
      .map_err(|e| format!("engine offline ({e})"))?;
    let mut reader = BufReader::new(pipe.try_clone().map_err(|e| e.to_string())?);
    let mut hello = String::new();
    reader.read_line(&mut hello).map_err(|e| e.to_string())?;
    writeln!(pipe, "{cmd}").map_err(|e| e.to_string())?;
    let mut resp = String::new();
    reader.read_line(&mut resp).map_err(|e| e.to_string())?;
    Ok(resp.trim().to_string())
  }
  #[cfg(not(windows))]
  {
    let _ = cmd;
    Err("Windows only".into())
  }
}

fn ensure_engine_process(app: &tauri::AppHandle) -> Result<(), String> {
  let mut eng = ENGINE.lock().map_err(|e| e.to_string())?;
  if let Some(child) = eng.child.as_mut() {
    if child.try_wait().map_err(|e| e.to_string())?.is_none() {
      return Ok(());
    }
  }

  let mut candidates = Vec::new();
  if let Ok(resource) = app.path().resource_dir() {
    candidates.push(resource.join("mb-engine-ipc.exe"));
  }
  candidates.push(std::path::PathBuf::from(
    r"C:\Users\scimo\Documents\CODE\MixBridge\native\audio-engine\build\mb-engine-ipc.exe",
  ));

  let mut started = None;
  for c in candidates {
    if c.exists() {
      let child = Command::new(&c)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .map_err(|e| format!("spawn ipc: {e}"))?;
      started = Some(child);
      break;
    }
  }
  let Some(child) = started else {
    return Err("mb-engine-ipc.exe not found".into());
  };
  eng.child = Some(child);
  std::thread::sleep(Duration::from_millis(400));
  Ok(())
}

#[tauri::command]
fn engine_status(app: tauri::AppHandle) -> Result<StatusDto, String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command("STATUS")?;
  let state = raw
    .split_whitespace()
    .skip_while(|t| *t != "STATE")
    .nth(1)
    .unwrap_or("unknown")
    .to_string();
  Ok(StatusDto { state, raw })
}

#[tauri::command]
fn engine_meter(app: tauri::AppHandle) -> Result<MeterDto, String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command("METER_MASTER")?;
  let mut peak = 0.0f32;
  let mut rms = 0.0f32;
  let mut clip = false;
  let parts: Vec<&str> = raw.split_whitespace().collect();
  for i in 0..parts.len() {
    if parts[i] == "PEAK" && i + 1 < parts.len() {
      peak = parts[i + 1].parse().unwrap_or(0.0);
    }
    if parts[i] == "RMS" && i + 1 < parts.len() {
      rms = parts[i + 1].parse().unwrap_or(0.0);
    }
    if parts[i] == "CLIP" && i + 1 < parts.len() {
      clip = parts[i + 1] == "1";
    }
  }
  Ok(MeterDto { peak, rms, clip })
}

#[tauri::command]
fn engine_start(app: tauri::AppHandle) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command("START")?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_stop(app: tauri::AppHandle) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command("STOP")?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_add_tone(app: tauri::AppHandle, hz: f32) -> Result<IdDto, String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("ADD_TONE {hz}"))?;
  if let Some(id) = raw.split_whitespace().last().and_then(|s| s.parse().ok()) {
    Ok(IdDto { id })
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_set_gain(app: tauri::AppHandle, id: u32, gain: f32) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_GAIN {id} {gain}"))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_set_mute(app: tauri::AppHandle, id: u32, mute: bool) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_MUTE {id} {}", if mute { 1 } else { 0 }))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .plugin(tauri_plugin_shell::init())
    .invoke_handler(tauri::generate_handler![
      engine_status,
      engine_meter,
      engine_start,
      engine_stop,
      engine_add_tone,
      engine_set_gain,
      engine_set_mute
    ])
    .run(tauri::generate_context!())
    .expect("error while running MixBridge");
}
