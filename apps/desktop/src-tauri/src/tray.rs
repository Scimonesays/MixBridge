use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicIsize, Ordering};
use std::sync::Mutex;
use std::time::Duration;

use serde::{Deserialize, Serialize};
use tauri::menu::{CheckMenuItem, IsMenuItem, Menu, MenuItem, PredefinedMenuItem, Submenu};
use tauri::tray::{MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Emitter, Manager, Wry};

#[derive(Debug)]
struct ShellError(String);

impl std::fmt::Display for ShellError {
  fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
    f.write_str(&self.0)
  }
}

impl std::error::Error for ShellError {}

#[derive(Clone, Serialize, Deserialize, Default)]
pub struct TraySource {
  pub id: u32,
  pub name: String,
  pub mute: bool,
  pub peak: f32,
}

#[derive(Clone, Serialize, Deserialize, Default)]
pub struct TraySnapshot {
  pub on_air: bool,
  pub monitor: String,
  pub live: String,
  pub sources: Vec<TraySource>,
}

static SNAPSHOT: Mutex<TraySnapshot> = Mutex::new(TraySnapshot {
  on_air: false,
  monitor: String::new(),
  live: String::new(),
  sources: Vec::new(),
});
static TRAY: Mutex<Option<TrayIcon>> = Mutex::new(None);
static MENU_SIGNATURE: Mutex<String> = Mutex::new(String::new());
static SHOW_EVENT: AtomicIsize = AtomicIsize::new(0);
static WINDOW_REQUESTED: AtomicBool = AtomicBool::new(false);

#[derive(Serialize, Deserialize)]
struct ShellPrefs {
  #[serde(default = "default_start")]
  start_with_windows: bool,
}

fn default_start() -> bool {
  true
}

fn snapshot() -> TraySnapshot {
  SNAPSHOT.lock().map(|g| g.clone()).unwrap_or_default()
}

fn menu_text(value: &str) -> String {
  value.replace('&', "&&")
}

fn short_label(value: &str, fallback: &str) -> String {
  let value = value.trim();
  let value = if value.is_empty() { fallback } else { value };
  let mut chars = value.chars();
  let clipped: String = chars.by_ref().take(42).collect();
  if chars.next().is_some() {
    menu_text(&format!("{clipped}…"))
  } else {
    menu_text(&clipped)
  }
}

fn coarse_peak(peak: f32) -> usize {
  let peak = peak.clamp(0.0, 1.0);
  if peak > 0.75 {
    3
  } else if peak > 0.35 {
    2
  } else if peak > 0.05 {
    1
  } else {
    0
  }
}

fn meter_text(peak: f32) -> String {
  let filled = coarse_peak(peak);
  format!("{}{}", "●".repeat(filled), "○".repeat(4 - filled))
}

fn prefs_path(app: &AppHandle) -> Result<PathBuf, String> {
  let dir = app.path().app_config_dir().map_err(|e| e.to_string())?;
  fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
  Ok(dir.join("shell.json"))
}

fn load_prefs(app: &AppHandle) -> ShellPrefs {
  let Ok(path) = prefs_path(app) else {
    return ShellPrefs { start_with_windows: true };
  };
  let Ok(data) = fs::read(&path) else {
    return ShellPrefs { start_with_windows: true };
  };
  serde_json::from_slice(&data).unwrap_or(ShellPrefs { start_with_windows: true })
}

fn save_prefs(app: &AppHandle, prefs: &ShellPrefs) -> Result<(), String> {
  let path = prefs_path(app)?;
  let data = serde_json::to_vec_pretty(prefs).map_err(|e| e.to_string())?;
  fs::write(path, data).map_err(|e| e.to_string())
}

fn apply_run_key(enabled: bool) -> Result<(), String> {
  let exe = std::env::current_exe().map_err(|e| e.to_string())?;
  let exe = exe.to_string_lossy().to_string();
  let mut cmd = std::process::Command::new("reg");
  if enabled {
    cmd.args([
      "add",
      r"HKCU\Software\Microsoft\Windows\CurrentVersion\Run",
      "/v",
      "MixBridge",
      "/t",
      "REG_SZ",
      "/d",
      &exe,
      "/f",
    ]);
  } else {
    cmd.args([
      "delete",
      r"HKCU\Software\Microsoft\Windows\CurrentVersion\Run",
      "/v",
      "MixBridge",
      "/f",
    ]);
  }
  #[cfg(windows)]
  {
    use std::os::windows::process::CommandExt;
    cmd.creation_flags(0x08000000);
  }
  let status = cmd.status().map_err(|e| e.to_string())?;
  if enabled && !status.success() {
    return Err(format!("autostart_failed_{}", status.code().unwrap_or(-1)));
  }
  Ok(())
}

pub fn boot_autostart(app: &AppHandle) {
  let prefs = load_prefs(app);
  let _ = save_prefs(app, &prefs);
  let _ = apply_run_key(prefs.start_with_windows);
}

fn set_autostart(app: &AppHandle, enabled: bool) -> Result<(), String> {
  save_prefs(app, &ShellPrefs { start_with_windows: enabled })?;
  apply_run_key(enabled)
}

fn menu_signature(app: &AppHandle) -> String {
  let snap = snapshot();
  let mut parts = vec![
    format!("air:{}", snap.on_air),
    format!("mon:{}", snap.monitor),
    format!("live:{}", snap.live),
    format!("auto:{}", load_prefs(app).start_with_windows),
  ];
  for src in &snap.sources {
    parts.push(format!(
      "src:{}:{}:{}:{}",
      src.id,
      src.name,
      src.mute,
      coarse_peak(src.peak)
    ));
  }
  if let Ok(names) = super::session_list(app.clone()) {
    parts.push(names.join("|"));
  }
  parts.join("\n")
}

fn build_menu(app: &AppHandle) -> Result<Menu<Wry>, String> {
  let snap = snapshot();
  let live = MenuItem::with_id(
    app,
    "toggle-live",
    if snap.on_air { "Standby" } else { "Go Live" },
    true,
    None::<&str>,
  )
  .map_err(|e| e.to_string())?;
  let sep_routes = PredefinedMenuItem::separator(app).map_err(|e| e.to_string())?;
  let inputs_head = MenuItem::with_id(app, "hdr-inputs", "Inputs", false, None::<&str>)
    .map_err(|e| e.to_string())?;

  let mut source_items = Vec::new();
  if snap.sources.is_empty() {
    source_items.push(
      MenuItem::with_id(app, "inputs-empty", "Saved session has no inputs yet", false, None::<&str>)
        .map_err(|e| e.to_string())?,
    );
  } else {
    for src in &snap.sources {
      let muted = if src.mute { "   muted" } else { "" };
      let label = format!("{}  {}{muted}", meter_text(src.peak), short_label(&src.name, "Input"));
      source_items.push(
        MenuItem::with_id(app, format!("mute:{}", src.id), label, true, None::<&str>)
          .map_err(|e| e.to_string())?,
      );
    }
  }

  let outputs_head = MenuItem::with_id(app, "hdr-outputs", "Outputs", false, None::<&str>)
    .map_err(|e| e.to_string())?;
  let monitor = MenuItem::with_id(
    app,
    "out-monitor",
    format!("Monitor   {}", short_label(&snap.monitor, "Not set")),
    false,
    None::<&str>,
  )
  .map_err(|e| e.to_string())?;
  let live_out = MenuItem::with_id(
    app,
    "out-live",
    format!("Live   {}", short_label(&snap.live, "Not set")),
    false,
    None::<&str>,
  )
  .map_err(|e| e.to_string())?;
  let sep_actions = PredefinedMenuItem::separator(app).map_err(|e| e.to_string())?;
  let open = MenuItem::with_id(app, "open", "Open mixer", true, None::<&str>).map_err(|e| e.to_string())?;

  let names = super::session_list(app.clone()).unwrap_or_default();
  let mut session_items = Vec::new();
  if names.is_empty() {
    session_items.push(
      MenuItem::with_id(app, "sessions-empty", "No saved sessions", false, None::<&str>)
        .map_err(|e| e.to_string())?,
    );
  } else {
    for name in &names {
      session_items.push(
        MenuItem::with_id(app, format!("load-session:{name}"), short_label(name, "Session"), true, None::<&str>)
          .map_err(|e| e.to_string())?,
      );
    }
  }
  let session_sep = PredefinedMenuItem::separator(app).map_err(|e| e.to_string())?;
  let manage = MenuItem::with_id(app, "sessions-manage", "Save or manage…", true, None::<&str>)
    .map_err(|e| e.to_string())?;
  let mut session_refs: Vec<&dyn IsMenuItem<Wry>> =
    session_items.iter().map(|item| item as &dyn IsMenuItem<Wry>).collect();
  session_refs.push(&session_sep);
  session_refs.push(&manage);
  let sessions = Submenu::with_id_and_items(app, "sessions", "Sessions", true, &session_refs)
    .map_err(|e| e.to_string())?;

  let sep_quit = PredefinedMenuItem::separator(app).map_err(|e| e.to_string())?;
  let autostart = CheckMenuItem::with_id(
    app,
    "autostart",
    "Start with Windows",
    true,
    load_prefs(app).start_with_windows,
    None::<&str>,
  )
  .map_err(|e| e.to_string())?;
  let quit = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>).map_err(|e| e.to_string())?;

  let mut items: Vec<&dyn IsMenuItem<Wry>> = vec![&live, &sep_routes, &inputs_head];
  for item in &source_items {
    items.push(item);
  }
  items.push(&outputs_head);
  items.push(&monitor);
  items.push(&live_out);
  items.push(&sep_actions);
  items.push(&open);
  items.push(&sessions);
  items.push(&sep_quit);
  items.push(&autostart);
  items.push(&quit);
  Menu::with_items(app, &items).map_err(|e| e.to_string())
}

pub fn refresh_menu(app: &AppHandle) {
  let signature = menu_signature(app);
  {
    let Ok(mut last) = MENU_SIGNATURE.lock() else { return };
    if *last == signature {
      return;
    }
    *last = signature;
  }
  let Ok(menu) = build_menu(app) else {
    if let Ok(mut last) = MENU_SIGNATURE.lock() {
      last.clear();
    }
    return;
  };
  let Ok(slot) = TRAY.lock() else { return };
  let Some(icon) = slot.clone() else { return };
  let snap = snapshot();
  let tip = if snap.on_air {
    "MixBridge — On Air"
  } else {
    "MixBridge — Standby"
  };
  let _ = icon.set_menu(Some(menu));
  let _ = icon.set_tooltip(Some(tip));
}

#[tauri::command]
pub fn tray_snapshot(app: AppHandle, snapshot: TraySnapshot) -> Result<(), String> {
  if let Ok(mut slot) = SNAPSHOT.lock() {
    *slot = snapshot;
  }
  refresh_menu(&app);
  Ok(())
}

pub fn show_mixer(app: &AppHandle) {
  WINDOW_REQUESTED.store(true, Ordering::SeqCst);
  let Some(window) = app.get_webview_window("main") else { return };
  let _ = window.set_skip_taskbar(false);
  let _ = window.unminimize();
  let _ = window.show();
  let _ = window.set_focus();
}

pub fn hide_mixer(app: &AppHandle) {
  WINDOW_REQUESTED.store(false, Ordering::SeqCst);
  conceal(app);
}

fn conceal(app: &AppHandle) {
  if WINDOW_REQUESTED.load(Ordering::SeqCst) {
    return;
  }
  let Some(window) = app.get_webview_window("main") else { return };
  if WINDOW_REQUESTED.load(Ordering::SeqCst) {
    return;
  }
  let _ = window.hide();
  let _ = window.set_skip_taskbar(true);
}

/// Windows can surface the webview after startup even when the config says hidden.
/// Keep it in the tray until the user opens it.
pub fn keep_tray_until_requested(app: AppHandle) {
  conceal(&app);
  // The webview is created hidden, then Windows shows it once the page is ready.
  // Pull it back to the tray until the user opens the mixer.
  std::thread::spawn(move || loop {
    std::thread::sleep(Duration::from_millis(100));
    if !WINDOW_REQUESTED.load(Ordering::SeqCst) {
      conceal(&app);
    }
  });
}

fn toggle_mixer(app: &AppHandle) {
  let Some(window) = app.get_webview_window("main") else { return };
  if window.is_visible().unwrap_or(false) {
    hide_mixer(app);
  } else {
    show_mixer(app);
  }
}

fn handle_menu(app: &AppHandle, id: &str) {
  if id == "toggle-live" {
    let live = super::engine_status(app.clone())
      .map(|status| status.broadcast.eq_ignore_ascii_case("live"))
      .unwrap_or(false);
    if live {
      let _ = super::broadcast_disable(app.clone());
    } else if let Err(err) = super::broadcast_enable(app.clone()) {
      show_mixer(app);
      let _ = app.emit("tray-error", err);
    }
    if let Ok(mut last) = MENU_SIGNATURE.lock() {
      last.clear();
    }
    refresh_menu(app);
    return;
  }
  if let Some(raw) = id.strip_prefix("mute:") {
    if let Ok(source_id) = raw.parse::<u32>() {
      let _ = app.emit("tray-toggle-mute", source_id);
    }
    return;
  }
  if let Some(name) = id.strip_prefix("load-session:") {
    let _ = app.emit("tray-load-session", name.to_string());
    return;
  }
  match id {
    "open" => show_mixer(app),
    "sessions-manage" => {
      show_mixer(app);
      let _ = app.emit("tray-sessions", true);
    }
    "autostart" => {
      let next = !load_prefs(app).start_with_windows;
      let _ = set_autostart(app, next);
      if let Ok(mut last) = MENU_SIGNATURE.lock() {
        last.clear();
      }
      refresh_menu(app);
    }
    "quit" => app.exit(0),
    _ => {}
  }
}

pub fn install(app: &tauri::App) -> Result<(), Box<dyn std::error::Error>> {
  let handle = app.handle().clone();
  let menu = build_menu(&handle).map_err(ShellError)?;
  let icon = app
    .default_window_icon()
    .cloned()
    .ok_or_else(|| ShellError("missing tray icon".into()))?;
  let tray = TrayIconBuilder::with_id("mixbridge")
    .tooltip("MixBridge — Standby")
    .icon(icon)
    .menu(&menu)
    .show_menu_on_left_click(false)
    .on_menu_event(|app, event| handle_menu(app, event.id.0.as_str()))
    .on_tray_icon_event(|tray, event| {
      if let TrayIconEvent::Click {
        button: MouseButton::Left,
        button_state: MouseButtonState::Up,
        ..
      } = event
      {
        toggle_mixer(tray.app_handle());
      }
    })
    .build(app)?;
  if let Ok(mut slot) = TRAY.lock() {
    *slot = Some(tray);
  }
  Ok(())
}

fn wide(value: &str) -> Vec<u16> {
  value.encode_utf16().chain(std::iter::once(0)).collect()
}

#[cfg(windows)]
#[link(name = "kernel32")]
extern "system" {
  fn CreateMutexW(attrs: *const u8, initial_owner: i32, name: *const u16) -> isize;
  fn CreateEventW(attrs: *const u8, manual_reset: i32, initial_state: i32, name: *const u16) -> isize;
  fn OpenEventW(access: u32, inherit: i32, name: *const u16) -> isize;
  fn SetEvent(handle: isize) -> i32;
  fn WaitForSingleObject(handle: isize, millis: u32) -> u32;
  fn CloseHandle(handle: isize) -> i32;
  fn GetLastError() -> u32;
}

pub fn claim_singleton() -> bool {
  #[cfg(windows)]
  {
    let name = wide("Local\\MixBridgeDesktop");
    unsafe {
      let handle = CreateMutexW(std::ptr::null(), 1, name.as_ptr());
      if handle == 0 {
        return true;
      }
      if GetLastError() == 183 {
        CloseHandle(handle);
        return false;
      }
      true
    }
  }
  #[cfg(not(windows))]
  {
    true
  }
}

pub fn create_show_event() {
  #[cfg(windows)]
  {
    let name = wide("Local\\MixBridgeShow");
    unsafe {
      let handle = CreateEventW(std::ptr::null(), 0, 0, name.as_ptr());
      SHOW_EVENT.store(handle, Ordering::SeqCst);
    }
  }
}

pub fn signal_show() {
  #[cfg(windows)]
  {
    let name = wide("Local\\MixBridgeShow");
    unsafe {
      let handle = OpenEventW(0x0002, 0, name.as_ptr());
      if handle != 0 {
        SetEvent(handle);
        CloseHandle(handle);
      }
    }
  }
}

pub fn listen_for_show(app: AppHandle) {
  let handle = SHOW_EVENT.load(Ordering::SeqCst);
  if handle == 0 {
    return;
  }
  std::thread::spawn(move || loop {
    let woke = unsafe { WaitForSingleObject(handle, 0xFFFFFFFF) };
    if woke == 0 {
      show_mixer(&app);
    } else {
      std::thread::sleep(Duration::from_secs(1));
    }
  });
}
