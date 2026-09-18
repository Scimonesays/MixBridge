use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tauri::Manager;

static ENGINE: Lazy<Mutex<EngineClient>> = Lazy::new(|| Mutex::new(EngineClient::default()));
static VST3_CACHE: Lazy<Mutex<Option<Vec<PluginDto>>>> = Lazy::new(|| Mutex::new(None));

#[derive(Default)]
struct EngineClient {
  child: Option<Child>,
}

#[derive(Serialize)]
struct StatusDto {
  state: String,
  broadcast: String,
  live_dest: bool,
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
  gain: f32,
  mute: bool,
  monitor: bool,
  broadcast: bool,
  effect_name: String,
  effect_path: String,
  effect_bypass: bool,
  effect_faulted: bool,
}

#[derive(Serialize, Clone)]
struct PluginDto {
  name: String,
  path: String,
}

#[derive(Serialize)]
struct StateFileDto {
  file: String,
}

#[derive(Serialize, Deserialize, Clone, Default)]
struct SessionSourceDto {
  kind: String,
  name: String,
  #[serde(default)]
  device_id: Option<String>,
  #[serde(default)]
  process_name: Option<String>,
  #[serde(default = "default_gain")]
  gain: f32,
  #[serde(default)]
  mute: bool,
  #[serde(default = "default_true")]
  monitor: bool,
  #[serde(default = "default_true")]
  broadcast: bool,
  #[serde(default)]
  fx: Vec<String>,
  #[serde(default)]
  fx_bypass: bool,
  #[serde(default)]
  fx_state_file: String,
}

#[derive(Serialize, Deserialize, Clone, Default)]
struct SessionDto {
  version: u32,
  name: String,
  #[serde(default)]
  monitor_device_id: String,
  #[serde(default)]
  live_device_id: String,
  #[serde(default)]
  sources: Vec<SessionSourceDto>,
}

fn default_gain() -> f32 { 1.0 }
fn default_true() -> bool { true }

fn vst3_name(path: &Path) -> String {
  path.file_stem()
    .and_then(|s| s.to_str())
    .unwrap_or("VST3")
    .to_string()
}

fn scan_vst3_folder(root: &Path, out: &mut Vec<PluginDto>) {
  let Ok(entries) = fs::read_dir(root) else { return; };
  for entry in entries.flatten() {
    let path = entry.path();
    let is_vst3 = path
      .extension()
      .and_then(|s| s.to_str())
      .map(|s| s.eq_ignore_ascii_case("vst3"))
      .unwrap_or(false);
    if is_vst3 {
      out.push(PluginDto {
        name: vst3_name(&path),
        path: path.to_string_lossy().to_string(),
      });
      continue;
    }
    if path.is_dir() {
      scan_vst3_folder(&path, out);
    }
  }
}

fn scan_vst3_plugins() -> Vec<PluginDto> {
  let mut roots = vec![
    PathBuf::from(r"C:\Program Files\Common Files\VST3"),
    PathBuf::from(r"C:\Program Files\VST3"),
  ];
  if let Some(pf) = std::env::var_os("ProgramFiles") {
    roots.push(PathBuf::from(pf).join("Common Files").join("VST3"));
  }
  if let Some(pd) = std::env::var_os("ProgramData") {
    roots.push(PathBuf::from(pd).join("VST3"));
  }
  if let Some(la) = std::env::var_os("LOCALAPPDATA") {
    roots.push(PathBuf::from(la).join("VST3"));
  }

  let mut out = Vec::new();
  for root in roots {
    scan_vst3_folder(&root, &mut out);
  }
  out.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()));
  out.dedup_by(|a, b| a.path.eq_ignore_ascii_case(&b.path));
  out
}

fn session_path(app: &tauri::AppHandle) -> Result<std::path::PathBuf, String> {
  let dir = app.path().app_config_dir().map_err(|e| e.to_string())?;
  fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
  Ok(dir.join("last-session.json"))
}

fn plugin_state_path(app: &tauri::AppHandle, file: &str) -> Result<PathBuf, String> {
  if file.is_empty() || Path::new(file).file_name().and_then(|v| v.to_str()) != Some(file) {
    return Err("invalid_plugin_state_file".into());
  }
  let dir = app.path().app_config_dir().map_err(|e| e.to_string())?.join("plugin-state");
  fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
  Ok(dir.join(file))
}

fn new_plugin_state_file(id: u32) -> String {
  let stamp = SystemTime::now()
    .duration_since(UNIX_EPOCH)
    .unwrap_or_default()
    .as_millis();
  format!("fx-{id}-{stamp}.mbfx")
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
  if let Some(override_path) = std::env::var_os("MIXBRIDGE_ENGINE_PATH") {
    candidates.push(PathBuf::from(override_path));
  }
  if let Ok(resource) = app.path().resource_dir() {
    candidates.push(resource.join("mb-engine-ipc.exe"));
  }
  if let Ok(cwd) = std::env::current_dir() {
    candidates.push(cwd.join("native").join("audio-engine").join("build").join("Release").join("mb-engine-ipc.exe"));
    candidates.push(cwd.join("native").join("audio-engine").join("build").join("mb-engine-ipc.exe"));
  }
  let manifest_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
  candidates.push(
    manifest_root
      .join("..")
      .join("..")
      .join("..")
      .join("native")
      .join("audio-engine")
      .join("build")
      .join("Release")
      .join("mb-engine-ipc.exe"),
  );

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
  Ok(StatusDto {
    state,
    broadcast,
    live_dest,
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
fn engine_set_monitor_device(app: tauri::AppHandle, device_id: String) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_MONITOR_DEVICE {device_id}"))?;
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
      // NAME ... GAIN
      let mut end = parts.len();
      for (j, t) in parts.iter().enumerate().skip(i + 1) {
        if *t == "GAIN" {
          end = j;
          break;
        }
      }
      parts[i + 1..end].join(" ")
    } else {
      String::new()
    };
    let gain = parse_field(&parts, "GAIN")
      .and_then(|s| s.parse().ok())
      .unwrap_or(1.0);
    let mute = parse_field(&parts, "MUTE") == Some("1");
    let monitor = parse_field(&parts, "MONITOR") != Some("0");
    let broadcast = parse_field(&parts, "BROADCAST") != Some("0");
    let effect_bypass = parse_field(&parts, "FX_BYPASS") == Some("1");
    let effect_faulted = parse_field(&parts, "FX_FAULT") == Some("1");
    let effect_name = if let Some(i) = parts.iter().position(|t| *t == "FX_NAME") {
      let end = parts.iter().enumerate().skip(i + 1)
        .find(|(_, t)| **t == "FX_PATH")
        .map(|(j, _)| j)
        .unwrap_or(parts.len());
      parts[i + 1..end].join(" ")
    } else {
      String::new()
    };
    let effect_path = if let Some(i) = parts.iter().position(|t| *t == "FX_PATH") {
      parts[i + 1..].join(" ")
    } else {
      String::new()
    };
    if id > 0 {
      out.push(SourceDto {
        id,
        kind,
        name,
        gain,
        mute,
        monitor,
        broadcast,
        effect_name,
        effect_path,
        effect_bypass,
        effect_faulted,
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
fn vst3_list(refresh: bool) -> Result<Vec<PluginDto>, String> {
  let mut cache = VST3_CACHE.lock().map_err(|e| e.to_string())?;
  if refresh || cache.is_none() {
    *cache = Some(scan_vst3_plugins());
  }
  Ok(cache.as_ref().cloned().unwrap_or_default())
}

#[tauri::command]
fn engine_set_vst3(app: tauri::AppHandle, id: u32, module_path: String) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("SET_FX {id} {module_path}"))?;
  if raw.starts_with("OK") { Ok(()) } else { Err(raw) }
}

#[tauri::command]
fn engine_clear_vst3(app: tauri::AppHandle, id: u32) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("CLEAR_FX {id}"))?;
  if raw.starts_with("OK") { Ok(()) } else { Err(raw) }
}

#[tauri::command]
fn engine_set_vst3_bypass(app: tauri::AppHandle, id: u32, bypass: bool) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let raw = pipe_command(&format!("FX_BYPASS {id} {}", if bypass { 1 } else { 0 }))?;
  if raw.starts_with("OK") { Ok(()) } else { Err(raw) }
}

#[tauri::command]
fn engine_save_vst3_state(
  app: tauri::AppHandle,
  id: u32,
  state_file: Option<String>,
) -> Result<StateFileDto, String> {
  ensure_engine_process(&app)?;
  let file = state_file.filter(|s| !s.is_empty()).unwrap_or_else(|| new_plugin_state_file(id));
  let path = plugin_state_path(&app, &file)?;
  let raw = pipe_command(&format!("SAVE_FX_STATE {id} {}", path.to_string_lossy()))?;
  if raw.starts_with("OK") {
    Ok(StateFileDto { file })
  } else {
    Err(raw)
  }
}

#[tauri::command]
fn engine_load_vst3_state(
  app: tauri::AppHandle,
  id: u32,
  state_file: String,
) -> Result<(), String> {
  ensure_engine_process(&app)?;
  let path = plugin_state_path(&app, &state_file)?;
  if !path.exists() {
    return Err("plugin_state_missing".into());
  }
  let raw = pipe_command(&format!("LOAD_FX_STATE {id} {}", path.to_string_lossy()))?;
  if raw.starts_with("OK") { Ok(()) } else { Err(raw) }
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

#[tauri::command]
fn session_save(app: tauri::AppHandle, session: SessionDto) -> Result<(), String> {
  if session.version != 1 {
    return Err(format!("unsupported_session_version_{}", session.version));
  }
  let path = session_path(&app)?;
  let data = serde_json::to_vec_pretty(&session).map_err(|e| e.to_string())?;
  fs::write(path, data).map_err(|e| e.to_string())
}

#[tauri::command]
fn session_load(app: tauri::AppHandle) -> Result<Option<SessionDto>, String> {
  let path = session_path(&app)?;
  let data = match fs::read(path) {
    Ok(data) => data,
    Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(None),
    Err(e) => return Err(e.to_string()),
  };
  let session: SessionDto = serde_json::from_slice(&data).map_err(|e| e.to_string())?;
  if session.version != 1 {
    return Err(format!("unsupported_session_version_{}", session.version));
  }
  Ok(Some(session))
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
      broadcast_enable,
      broadcast_disable,
      engine_list_capture,
      engine_list_render,
      engine_list_processes,
      engine_list_sources,
      engine_set_live_device,
      engine_set_monitor_device,
      engine_add_physical,
      engine_add_process,
      engine_add_tone,
      engine_remove_source,
      vst3_list,
      engine_set_vst3,
      engine_clear_vst3,
      engine_set_vst3_bypass,
      engine_save_vst3_state,
      engine_load_vst3_state,
      engine_set_gain,
      engine_set_mute,
      engine_set_monitor,
      engine_set_broadcast,
      session_save,
      session_load
    ])
    .run(tauri::generate_context!())
    .expect("error while running MixBridge");
}
