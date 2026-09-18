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
  broadcast: String,
  live_dest: bool,
  frames: u64,
  xruns: u64,
  underruns: u64,
  overruns: u64,
  latency_ms: f32,
  feedback: f32,
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

#[derive(Serialize, Clone)]
struct DeviceDto {
  id: String,
  name: String,
}

#[derive(Serialize, Clone)]
struct ProcessDto {
  pid: u32,
  name: String,
}

#[derive(Serialize, Clone)]
struct SourceDto {
  id: u32,
  kind: String,
  name: String,
  device_id: String,
  gain: f32,
  mute: bool,
  monitor: bool,
  broadcast: bool,
  fx: String,
}

fn open_pipe() -> Result<(std::fs::File, BufReader<std::fs::File>), String> {
  #[cfg(windows)]
  {
    use std::fs::OpenOptions;
    let mut last_err = String::new();
    for _ in 0..8 {
      match OpenOptions::new()
        .read(true)
        .write(true)
        .open(r"\\.\pipe\mixbridge-engine")
      {
        Ok(pipe) => {
          let reader = BufReader::new(pipe.try_clone().map_err(|e| e.to_string())?);
          return Ok((pipe, reader));
        }
        Err(e) => {
          last_err = e.to_string();
          std::thread::sleep(Duration::from_millis(80));
        }
      }
    }
    Err(format!("engine offline ({last_err})"))
  }
  #[cfg(not(windows))]
  {
    Err("Windows only".into())
  }
}

fn pipe_command(cmd: &str) -> Result<String, String> {
  let (mut pipe, mut reader) = open_pipe()?;
  let mut hello = String::new();
  reader.read_line(&mut hello).map_err(|e| e.to_string())?;
  writeln!(pipe, "{cmd}").map_err(|e| e.to_string())?;
  let mut resp = String::new();
  reader.read_line(&mut resp).map_err(|e| e.to_string())?;
  Ok(resp.trim().to_string())
}

fn pipe_command_until_end(cmd: &str) -> Result<Vec<String>, String> {
  let (mut pipe, mut reader) = open_pipe()?;
  let mut hello = String::new();
  reader.read_line(&mut hello).map_err(|e| e.to_string())?;
  writeln!(pipe, "{cmd}").map_err(|e| e.to_string())?;
  let mut lines = Vec::new();
  loop {
    let mut resp = String::new();
    reader.read_line(&mut resp).map_err(|e| e.to_string())?;
    let t = resp.trim().to_string();
    if t.is_empty() {
      break;
    }
    let done = t == "OK END" || t.starts_with("ERR ");
    lines.push(t);
    if done {
      break;
    }
  }
  Ok(lines)
}

fn parse_field<'a>(parts: &[&'a str], key: &str) -> Option<&'a str> {
  parts
    .iter()
    .position(|t| *t == key)
    .and_then(|i| parts.get(i + 1).copied())
}

fn parse_meter(raw: &str) -> MeterDto {
  let parts: Vec<&str> = raw.split_whitespace().collect();
  MeterDto {
    peak: parse_field(&parts, "PEAK")
      .and_then(|s| s.parse().ok())
      .unwrap_or(0.0),
    rms: parse_field(&parts, "RMS")
      .and_then(|s| s.parse().ok())
      .unwrap_or(0.0),
    clip: parse_field(&parts, "CLIP") == Some("1"),
  }
}

fn parse_id(raw: &str) -> Result<IdDto, String> {
  let parts: Vec<&str> = raw.split_whitespace().collect();
  if let Some(id) = parse_field(&parts, "ID").and_then(|s| s.parse().ok()) {
    Ok(IdDto { id })
  } else {
    Err(raw.to_string())
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
  std::thread::sleep(Duration::from_millis(500));
  Ok(())
}

fn force_respawn(app: &tauri::AppHandle) -> Result<(), String> {
  {
    let mut eng = ENGINE.lock().map_err(|e| e.to_string())?;
    if let Some(mut child) = eng.child.take() {
      let _ = child.kill();
      let _ = child.wait();
    }
  }
  ensure_engine_process(app)
}

fn ensure_running(app: &tauri::AppHandle) -> Result<(), String> {
  ensure_engine_process(app)?;
  let raw = pipe_command("START")?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_status(app: tauri::AppHandle) -> Result<StatusDto, String> {
  ensure_engine_process(&app)?;
  let raw = match pipe_command("STATUS") {
    Ok(v) => v,
    Err(_) => {
      force_respawn(&app)?;
      pipe_command("STATUS")?
    }
  };
  let parts: Vec<&str> = raw.split_whitespace().collect();
  let state = parse_field(&parts, "STATE").unwrap_or("unknown").to_string();
  let broadcast = parse_field(&parts, "BROADCAST")
    .unwrap_or("standby")
    .to_string();
  let live_dest = parse_field(&parts, "LIVE_DEST") == Some("1");
  let frames = parse_field(&parts, "FRAMES")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0);
  let xruns = parse_field(&parts, "XRUNS")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0);
  let underruns = parse_field(&parts, "UNDERRUNS")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0);
  let overruns = parse_field(&parts, "OVERRUNS")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0);
  let latency_ms = parse_field(&parts, "LAT_MS")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0.0);
  let feedback = parse_field(&parts, "FEEDBACK")
    .and_then(|s| s.parse().ok())
    .unwrap_or(0.0);
  Ok(StatusDto {
    state,
    broadcast,
    live_dest,
    frames,
    xruns,
    underruns,
    overruns,
    latency_ms,
    feedback,
    raw,
  })
}

#[tauri::command]
fn engine_ensure_running(app: tauri::AppHandle) -> Result<(), String> {
  ensure_running(&app)
}

#[tauri::command]
fn engine_meter(app: tauri::AppHandle) -> Result<MeterDto, String> {
  ensure_engine_process(&app)?;
  Ok(parse_meter(&pipe_command("METER_MASTER")?))
}

#[tauri::command]
fn engine_meter_broadcast(app: tauri::AppHandle) -> Result<MeterDto, String> {
  ensure_engine_process(&app)?;
  Ok(parse_meter(&pipe_command("METER_BROADCAST")?))
}

#[tauri::command]
fn engine_meter_source(app: tauri::AppHandle, id: u32) -> Result<MeterDto, String> {
  ensure_engine_process(&app)?;
  Ok(parse_meter(&pipe_command(&format!("METER_SOURCE {id}"))?))
}

#[tauri::command]
fn engine_start(app: tauri::AppHandle) -> Result<(), String> {
  ensure_running(&app)
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
fn engine_restart(app: tauri::AppHandle) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = match pipe_command("RESTART") {
    Ok(v) if v.starts_with("OK") => v,
    _ => {
      force_respawn(&app)?;
      pipe_command("RESTART").unwrap_or_else(|_| "OK".into())
    }
  };
  if raw.starts_with("OK") || raw == "OK" {
    let _ = pipe_command("START");
    Ok(())
  } else {
    force_respawn(&app)?;
    ensure_running(&app)
  }
}

#[tauri::command]
fn broadcast_enable(app: tauri::AppHandle) -> Result<(), String> {
  ensure_running(&app)?;
  let raw = pipe_command("BROADCAST_ENABLE")?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn broadcast_disable(app: tauri::AppHandle) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command("BROADCAST_DISABLE")?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_list_capture(app: tauri::AppHandle) -> Result<Vec<DeviceDto>, String> {
  ensure_engine_process(&app)?;
  let lines = pipe_command_until_end("LIST_CAPTURE")?;
  let mut out = Vec::new();
  for line in lines {
    if !line.starts_with("DEVICE ") {
      continue;
    }
    if let Some(rest) = line.strip_prefix("DEVICE ID ") {
      if let Some((id, name)) = rest.split_once(" NAME ") {
        out.push(DeviceDto {
          id: id.to_string(),
          name: name.to_string(),
        });
      }
    }
  }
  Ok(out)
}

#[tauri::command]
fn engine_list_render(app: tauri::AppHandle) -> Result<Vec<DeviceDto>, String> {
  ensure_engine_process(&app)?;
  let lines = pipe_command_until_end("LIST_RENDER")?;
  let mut out = Vec::new();
  for line in lines {
    if !line.starts_with("DEVICE ") {
      continue;
    }
    if let Some(rest) = line.strip_prefix("DEVICE ID ") {
      if let Some((id, name)) = rest.split_once(" NAME ") {
        out.push(DeviceDto {
          id: id.to_string(),
          name: name.to_string(),
        });
      }
    }
  }
  Ok(out)
}

#[tauri::command]
fn engine_set_live_device(app: tauri::AppHandle, device_id: String) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_LIVE_DEVICE {device_id}"))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_list_processes(app: tauri::AppHandle) -> Result<Vec<ProcessDto>, String> {
  ensure_engine_process(&app)?;
  let lines = pipe_command_until_end("LIST_PROCESSES")?;
  let mut out = Vec::new();
  for line in lines {
    if !line.starts_with("PROCESS ") {
      continue;
    }
    let parts: Vec<&str> = line.split_whitespace().collect();
    let pid = parse_field(&parts, "PID")
      .and_then(|s| s.parse().ok())
      .unwrap_or(0);
    let name = if let Some(i) = parts.iter().position(|t| *t == "NAME") {
      parts[i + 1..].join(" ")
    } else {
      String::new()
    };
    if pid > 0 && !name.is_empty() {
      out.push(ProcessDto { pid, name });
    }
  }
  Ok(out)
}

#[tauri::command]
fn engine_list_sources(app: tauri::AppHandle) -> Result<Vec<SourceDto>, String> {
  ensure_engine_process(&app)?;
  let lines = pipe_command_until_end("LIST_SOURCES")?;
  let mut out = Vec::new();
  for line in lines {
    if !line.starts_with("SOURCE ") {
      continue;
    }
    let parts: Vec<&str> = line.split_whitespace().collect();
    let id = parse_field(&parts, "ID")
      .and_then(|s| s.parse().ok())
      .unwrap_or(0);
    let kind = parse_field(&parts, "KIND").unwrap_or("unknown").to_string();
    let name = if let Some(i) = parts.iter().position(|t| *t == "NAME") {
      let mut end = parts.len();
      for (j, t) in parts.iter().enumerate().skip(i + 1) {
        if *t == "DEVICE" || *t == "GAIN" {
          end = j;
          break;
        }
      }
      parts[i + 1..end].join(" ")
    } else {
      String::new()
    };
    let device_id = parse_field(&parts, "DEVICE")
      .filter(|s| *s != "-")
      .unwrap_or("")
      .to_string();
    let gain = parse_field(&parts, "GAIN")
      .and_then(|s| s.parse().ok())
      .unwrap_or(1.0);
    let mute = parse_field(&parts, "MUTE") == Some("1");
    let monitor = parse_field(&parts, "MONITOR") != Some("0");
    let broadcast = parse_field(&parts, "BROADCAST") != Some("0");
    let fx = if let Some(i) = parts.iter().position(|t| *t == "FX") {
      parts[i + 1..].join(" ")
    } else {
      String::new()
    };
    if id > 0 {
      out.push(SourceDto {
        id,
        kind,
        name,
        device_id,
        gain,
        mute,
        monitor,
        broadcast,
        fx,
      });
    }
  }
  Ok(out)
}

#[tauri::command]
fn engine_add_physical(app: tauri::AppHandle, device_id: String) -> Result<IdDto, String> {
  ensure_running(&app)?;
  parse_id(&pipe_command(&format!("ADD_PHYSICAL {device_id}"))?)
}

#[tauri::command]
fn engine_add_process(app: tauri::AppHandle, pid: u32, name: String) -> Result<IdDto, String> {
  ensure_running(&app)?;
  parse_id(&pipe_command(&format!("ADD_PROCESS {pid} {name}"))?)
}

#[tauri::command]
fn engine_add_tone(app: tauri::AppHandle, hz: f32) -> Result<IdDto, String> {
  ensure_running(&app)?;
  parse_id(&pipe_command(&format!("ADD_TONE {hz}"))?)
}

#[tauri::command]
fn engine_remove_source(app: tauri::AppHandle, id: u32) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("REMOVE {id}"))?;
  if raw.starts_with("OK") {
    Ok(())
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

#[tauri::command]
fn engine_set_monitor(app: tauri::AppHandle, id: u32, enabled: bool) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!(
    "SET_MONITOR {id} {}",
    if enabled { 1 } else { 0 }
  ))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_set_broadcast(app: tauri::AppHandle, id: u32, enabled: bool) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!(
    "SET_BROADCAST {id} {}",
    if enabled { 1 } else { 0 }
  ))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[derive(Serialize, Clone)]
struct PluginDto {
  name: String,
  path: String,
  quarantine: bool,
}

#[tauri::command]
fn engine_list_vst3(app: tauri::AppHandle) -> Result<Vec<PluginDto>, String> {
  ensure_engine_process(&app)?;
  let lines = pipe_command_until_end("LIST_VST3")?;
  let mut out = Vec::new();
  for line in lines {
    if !line.starts_with("PLUGIN ") {
      continue;
    }
    // PLUGIN NAME <name...> QUARANTINE <0|1> PATH <path...>
    if let Some(rest) = line.strip_prefix("PLUGIN NAME ") {
      if let Some((name_q, path)) = rest.split_once(" PATH ") {
        if let Some((name, q)) = name_q.rsplit_once(" QUARANTINE ") {
          out.push(PluginDto {
            name: name.to_string(),
            path: path.to_string(),
            quarantine: q.trim() == "1",
          });
        }
      }
    }
  }
  Ok(out)
}

#[tauri::command]
fn engine_add_fx(app: tauri::AppHandle, id: u32, path: String) -> Result<String, String> {
  ensure_running(&app)?;
  let raw = pipe_command(&format!("ADD_FX {id} {path}"))?;
  if raw.starts_with("OK") {
    Ok(raw)
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_remove_fx(app: tauri::AppHandle, id: u32) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("REMOVE_FX {id}"))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_set_fx_bypass(app: tauri::AppHandle, id: u32, bypass: bool) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!(
    "SET_FX_BYPASS {id} {}",
    if bypass { 1 } else { 0 }
  ))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_save_fx_state(app: tauri::AppHandle, id: u32, index: u32) -> Result<String, String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("GET_FX_STATE {id} {index}"))?;
  if !raw.starts_with("OK") {
    return Err(raw);
  }
  let parts: Vec<&str> = raw.split_whitespace().collect();
  parse_field(&parts, "PATH")
    .map(|s| s.to_string())
    .ok_or_else(|| raw.clone())
}

#[tauri::command]
fn engine_load_fx_state(app: tauri::AppHandle, id: u32, index: u32, path: String) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_FX_STATE {id} {index} {path}"))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_open_fx_editor(app: tauri::AppHandle, id: u32) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("OPEN_FX_EDITOR {id}"))?;
  if raw.starts_with("OK") {
    Ok(())
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn session_save(name: String, json: String) -> Result<String, String> {
  let base = std::env::var("LOCALAPPDATA").map_err(|e| e.to_string())?;
  let dir = std::path::PathBuf::from(base).join("MixBridge").join("presets");
  std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
  let safe: String = name
    .chars()
    .map(|c| if c.is_ascii_alphanumeric() || c == '-' || c == '_' || c == ' ' {
      c
    } else {
      '_'
    })
    .collect();
  let path = dir.join(format!("{safe}.json"));
  std::fs::write(&path, json).map_err(|e| e.to_string())?;
  Ok(path.to_string_lossy().to_string())
}

#[tauri::command]
fn session_load(name: String) -> Result<String, String> {
  let base = std::env::var("LOCALAPPDATA").map_err(|e| e.to_string())?;
  let path = std::path::PathBuf::from(base)
    .join("MixBridge")
    .join("presets")
    .join(format!("{name}.json"));
  std::fs::read_to_string(path).map_err(|e| e.to_string())
}

#[tauri::command]
fn session_list() -> Result<Vec<String>, String> {
  let base = std::env::var("LOCALAPPDATA").map_err(|e| e.to_string())?;
  let dir = std::path::PathBuf::from(base).join("MixBridge").join("presets");
  if !dir.exists() {
    return Ok(vec![]);
  }
  let mut out = Vec::new();
  for e in std::fs::read_dir(dir).map_err(|e| e.to_string())? {
    let e = e.map_err(|e| e.to_string())?;
    let p = e.path();
    if p.extension().and_then(|s| s.to_str()) == Some("json") {
      if let Some(stem) = p.file_stem().and_then(|s| s.to_str()) {
        out.push(stem.to_string());
      }
    }
  }
  out.sort();
  Ok(out)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .plugin(tauri_plugin_shell::init())
    .invoke_handler(tauri::generate_handler![
      engine_status,
      engine_ensure_running,
      engine_meter,
      engine_meter_broadcast,
      engine_meter_source,
      engine_start,
      engine_stop,
      engine_restart,
      broadcast_enable,
      broadcast_disable,
      engine_list_capture,
      engine_list_render,
      engine_list_processes,
      engine_list_sources,
      engine_list_vst3,
      engine_set_live_device,
      engine_add_physical,
      engine_add_process,
      engine_add_tone,
      engine_remove_source,
      engine_add_fx,
      engine_remove_fx,
      engine_set_fx_bypass,
      engine_open_fx_editor,
      engine_save_fx_state,
      engine_load_fx_state,
      engine_set_gain,
      engine_set_mute,
      engine_set_monitor,
      engine_set_broadcast,
      session_save,
      session_load,
      session_list
    ])
    .run(tauri::generate_context!())
    .expect("error while running MixBridge");
}
