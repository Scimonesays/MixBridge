const airBadge = document.getElementById("air-badge");
const meterEl = document.getElementById("meter");
const monitorMeterEl = document.getElementById("monitor-meter");
const monitorRoute = document.getElementById("monitor-route");
const monitorRouteLabel = document.getElementById("monitor-route-label");
const liveMeterEl = document.getElementById("live-meter");
const liveRoute = document.getElementById("live-route");
const liveRouteLabel = document.getElementById("live-route-label");
const statusEl = document.getElementById("status");
const btnLive = document.getElementById("btn-live");
const liveLabel = document.getElementById("live-label");
const sourceList = document.getElementById("source-list");
const btnAdd = document.getElementById("btn-add");
const picker = document.getElementById("picker");
const pickerRoot = document.getElementById("picker-root");
const instrumentDialog = document.getElementById("instrument-dialog");
const instrumentKeys = document.getElementById("instrument-keys");
const instrumentTitle = document.getElementById("instrument-title");
const instrumentClose = document.getElementById("instrument-close");

/** @type {Map<number, {id:number, kind:string, name:string, mute:boolean, monitor:boolean, broadcast:boolean, gain:number, deviceId?:string, inputChannel?:number|null, processName?:string, instrumentPreset?:number, effectName?:string, effectPath?:string, effectBypass?:boolean, effectFaulted?:boolean, effectStateFile?:string, effectEditorOpen?:boolean, effectDirty?:boolean}>} */
const sources = new Map();

let onAir = false;
let liveDestReady = false;
let liveDestName = "";
let liveDestId = "";
let monitorDestName = "Monitor";
let monitorDestId = "";
let engineOnline = false;
let restoringSession = false;
let restoreInFlight = false;
let sessionSaveTimer = null;
let pendingRestores = [];

const ICON = {
  mic: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 3a3 3 0 0 1 3 3v6a3 3 0 1 1-6 0V6a3 3 0 0 1 3-3zm-7 9a1 1 0 0 1 2 0 5 5 0 0 0 10 0 1 1 0 1 1 2 0 7 7 0 0 1-6 6.93V21h3a1 1 0 1 1 0 2H8a1 1 0 1 1 0-2h3v-2.07A7 7 0 0 1 5 12z"/></svg>`,
  app: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M4 4h7v7H4V4zm9 0h7v7h-7V4zM4 13h7v7H4v-7zm9 0h7v7h-7v-7z"/></svg>`,
  headphones: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 3a9 9 0 0 0-9 9v7a2 2 0 0 0 2 2h2v-8H5a7 7 0 0 1 14 0h-2v8h2a2 2 0 0 0 2-2v-7a9 9 0 0 0-9-9z"/></svg>`,
  broadcast: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 10a2 2 0 1 1 0 4 2 2 0 0 1 0-4zm-5.5-1.5a1 1 0 0 1 1.4 1.45 4 4 0 0 0 0 5.7 1 1 0 1 1-1.4 1.4 6 6 0 0 1 0-8.55zm11 0a6 6 0 0 1 0 8.55 1 1 0 1 1-1.4-1.4 4 4 0 0 0 0-5.7 1 1 0 0 1 1.4-1.45z"/></svg>`,
  mute: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M5 9v6h4l5 5V4L9 9H5zm12.5 3a4.5 4.5 0 0 0-2.5-4.03v8.06A4.5 4.5 0 0 0 17.5 12z"/></svg>`,
  trash: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M9 3h6l1 2h4v2H4V5h4l1-2zm1 6h2v10h-2V9zm4 0h2v10h-2V9zM7 9h2v10H7V9z"/></svg>`,
  fx: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M8 2h2v5h4V2h2v5h2v5a6 6 0 0 1-5 5.92V22h-2v-4.08A6 6 0 0 1 6 12V7h2V2zm0 7v3a4 4 0 0 0 8 0V9H8z"/></svg>`,
  power: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M11 2h2v10h-2V2zm-4.95 3.64 1.41 1.42A7 7 0 1 0 16.54 7l1.41-1.42A9 9 0 1 1 6.05 5.64z"/></svg>`,
  window: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M4 5h16v14H4V5zm2 3v9h12V8H6zm1-2h2v1H7V6zm3 0h2v1h-2V6z"/></svg>`,
  back: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M14.7 5.3 8 12l6.7 6.7 1.4-1.4L10.8 12l5.3-5.3-1.4-1.4z"/></svg>`,
  keys: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M3 5h18v14H3V5zm2 2v10h2V7H5zm4 0v6h2V7H9zm4 0v10h2V7h-2zm4 0v6h2V7h-2z"/></svg>`,
};

function showError(msg) {
  statusEl.textContent = msg || "";
}

function setMeterWidth(el, peak) {
  const pct = Math.min(100, Math.round((peak || 0) * 140));
  el.style.width = `${pct}%`;
  return pct;
}

function setMeterVertical(el, peak) {
  const pct = Math.min(100, Math.round((peak || 0) * 140));
  if (window.matchMedia("(max-width: 720px)").matches) {
    el.style.width = `${pct}%`;
    el.style.height = "100%";
  } else {
    el.style.height = `${pct}%`;
    el.style.width = "100%";
  }
  return pct;
}

function setAirVisual(live) {
  onAir = live;
  airBadge.textContent = live ? "On Air" : "Standby";
  airBadge.classList.toggle("on-air", live);
  airBadge.classList.remove("offline");
  btnLive.classList.toggle("standby", !live);
  btnLive.setAttribute("aria-pressed", live ? "true" : "false");
  btnLive.setAttribute("aria-label", live ? "Standby" : "Go Live");
  btnLive.title = live ? "Standby" : "Go Live";
  liveLabel.textContent = live ? "On Air" : "Go Live";
  liveRoute.classList.toggle("dim", !live);
  liveRoute.classList.toggle("live-active", live);
  liveRoute.classList.toggle("unavailable", !liveDestReady);
  liveRouteLabel.textContent = liveDestName || "Live";
  liveRoute.title = liveDestName ? `Live · ${liveDestName}` : "Choose live output";
  // Do not fake Live availability.
  btnLive.disabled = false;
}

async function invoke(cmd, args = {}) {
  if (!window.__TAURI_INTERNALS__) {
    return Promise.reject(new Error("Tauri runtime required"));
  }
  const { invoke } = await import("@tauri-apps/api/core");
  return invoke(cmd, args);
}

function normalizeProcessName(name) {
  return String(name || "").replace(/\.exe$/i, "").trim().toLowerCase();
}

function sessionKey(s) {
  if (s.kind === "physical") {
    const channel = s.input_channel ?? s.inputChannel ?? -1;
    return "physical:" + (s.device_id || s.deviceId || s.name || "") + ":" + channel;
  }
  if (s.kind === "process") return "process:" + normalizeProcessName(s.process_name || s.processName || s.name);
  return s.kind + ":" + (s.name || "");
}

function pluginNameFromPath(path) {
  const normalized = String(path || "").replace(/\\/g, "/");
  const tail = normalized.split("/").pop() || "VST3";
  return tail.replace(/\.vst3$/i, "");
}

function sourceToSession(src) {
  return {
    kind: src.kind,
    name: src.name,
    device_id: src.deviceId || null,
    process_name: src.processName || null,
    input_channel: Number.isInteger(src.inputChannel) ? src.inputChannel : null,
    gain: src.gain,
    mute: src.mute,
    monitor: src.monitor,
    broadcast: src.broadcast,
    fx: src.effectPath ? [src.effectPath] : [],
    fx_bypass: !!src.effectBypass,
    fx_state_file: src.effectStateFile || "",
    instrument_preset: Number(src.instrumentPreset || 0),
  };
}

function buildSession() {
  const active = [...sources.values()]
    .filter((src) => src.kind === "physical" || src.kind === "process" || src.kind === "instrument")
    .map(sourceToSession);
  const seen = new Set(active.map(sessionKey));
  for (const pending of pendingRestores) {
    if (!seen.has(sessionKey(pending))) active.push(pending);
  }
  return {
    version: 1,
    name: "Discord Jam",
    monitor_device_id: monitorDestId,
    live_device_id: liveDestId,
    sources: active,
  };
}

async function captureEffectState(src) {
  if (!src?.effectPath) return;
  try {
    const result = await invoke("engine_save_vst3_state", {
      id: src.id,
      stateFile: src.effectStateFile || null,
    });
    src.effectStateFile = result.file;
  } catch (e) {
    console.warn("MixBridge plugin state save failed", e);
  }
}

async function saveSessionNow() {
  if (restoringSession) return;
  try {
    await invoke("session_save", { session: buildSession() });
  } catch (e) {
    console.warn("MixBridge session save failed", e);
  }
}

function scheduleSessionSave() {
  if (restoringSession) return;
  clearTimeout(sessionSaveTimer);
  sessionSaveTimer = setTimeout(saveSessionNow, 250);
}

async function applySourceSettings(id, cfg) {
  await invoke("engine_set_gain", { id, gain: Number(cfg.gain ?? 1) });
  await invoke("engine_set_mute", { id, mute: !!cfg.mute });
  await invoke("engine_set_monitor", { id, enabled: cfg.monitor !== false });
  await invoke("engine_set_broadcast", { id, enabled: cfg.broadcast !== false });
  if (Array.isArray(cfg.fx) && cfg.fx[0]) {
    await invoke("engine_set_vst3", { id, modulePath: cfg.fx[0] });
    if (cfg.fx_state_file) {
      try {
        await invoke("engine_load_vst3_state", { id, stateFile: cfg.fx_state_file });
      } catch (e) {
        console.warn("MixBridge plugin state restore failed", e);
      }
    }
    if (cfg.fx_bypass) {
      await invoke("engine_set_vst3_bypass", { id, bypass: true });
    }
  }
}

async function tryRestorePendingSources() {
  if (restoreInFlight || pendingRestores.length === 0) return;
  restoreInFlight = true;
  try {
    const devices = await invoke("engine_list_capture");
    const processes = await invoke("engine_list_processes");
    const remaining = [];

    for (const cfg of pendingRestores) {
      try {
        if (cfg.kind === "instrument") {
          const res = await invoke("engine_add_instrument", { preset: Number(cfg.instrument_preset || 0) });
          await applySourceSettings(res.id, cfg);
          upsertSource({
            id: res.id,
            kind: "instrument",
            name: cfg.name || (Number(cfg.instrument_preset || 0) === 1 ? "Soft Pad" : "Neon Keys"),
            gain: Number(cfg.gain ?? 1),
            mute: !!cfg.mute,
            monitor: cfg.monitor !== false,
            broadcast: cfg.broadcast !== false,
            instrumentPreset: Number(cfg.instrument_preset || 0),
            effectPath: Array.isArray(cfg.fx) ? (cfg.fx[0] || "") : "",
            effectName: Array.isArray(cfg.fx) && cfg.fx[0] ? pluginNameFromPath(cfg.fx[0]) : "",
            effectBypass: !!cfg.fx_bypass,
            effectStateFile: cfg.fx_state_file || "",
          });
          continue;
        }

        if (cfg.kind === "physical") {
          const device =
            devices.find((d) => cfg.device_id && d.id === cfg.device_id) ||
            devices.find((d) => d.name === cfg.name);
          if (!device) {
            remaining.push(cfg);
            continue;
          }
          const res = await invoke("engine_add_physical", {
            deviceId: device.id,
            inputChannel: cfg.input_channel ?? null,
          });
          await applySourceSettings(res.id, cfg);
          upsertSource({
            id: res.id,
            kind: "physical",
            name: cfg.name || device.name,
            inputChannel: cfg.input_channel ?? null,
            gain: Number(cfg.gain ?? 1),
            mute: !!cfg.mute,
            monitor: cfg.monitor !== false,
            broadcast: cfg.broadcast !== false,
            deviceId: device.id,
            effectPath: Array.isArray(cfg.fx) ? (cfg.fx[0] || "") : "",
            effectName: Array.isArray(cfg.fx) && cfg.fx[0] ? pluginNameFromPath(cfg.fx[0]) : "",
            effectBypass: !!cfg.fx_bypass,
            effectStateFile: cfg.fx_state_file || "",
          });
          continue;
        }

        if (cfg.kind === "process") {
          const wanted = normalizeProcessName(cfg.process_name || cfg.name);
          const process = processes.find((p) => normalizeProcessName(p.name) === wanted);
          if (!process) {
            remaining.push(cfg);
            continue;
          }
          const label = String(process.name).replace(/\.exe$/i, "");
          const res = await invoke("engine_add_process", { pid: process.pid, name: label });
          await applySourceSettings(res.id, cfg);
          upsertSource({
            id: res.id,
            kind: "process",
            name: cfg.name || label,
            gain: Number(cfg.gain ?? 1),
            mute: !!cfg.mute,
            monitor: cfg.monitor !== false,
            broadcast: cfg.broadcast !== false,
            processName: label,
            effectPath: Array.isArray(cfg.fx) ? (cfg.fx[0] || "") : "",
            effectName: Array.isArray(cfg.fx) && cfg.fx[0] ? pluginNameFromPath(cfg.fx[0]) : "",
            effectBypass: !!cfg.fx_bypass,
            effectStateFile: cfg.fx_state_file || "",
          });
          continue;
        }
      } catch (e) {
        console.warn("MixBridge source restore deferred", e);
      }
      remaining.push(cfg);
    }

    pendingRestores = remaining;
    renderSources();
  } finally {
    restoreInFlight = false;
  }
}

async function restoreLastSession() {
  restoringSession = true;
  try {
    const session = await invoke("session_load");
    if (!session) return;

    const renderDevices = await invoke("engine_list_render");
    if (session.monitor_device_id) {
      const monitor = renderDevices.find((d) => d.id === session.monitor_device_id);
      if (monitor) {
        await invoke("engine_set_monitor_device", { deviceId: monitor.id });
        monitorDestId = monitor.id;
        monitorDestName = monitor.name;
        monitorRouteLabel.textContent = monitor.name;
        monitorRoute.title = "Monitor · " + monitor.name;
      }
    }

    if (session.live_device_id) {
      const live = renderDevices.find((d) => d.id === session.live_device_id);
      if (live) {
        await invoke("engine_set_live_device", { deviceId: live.id });
        liveDestId = live.id;
        liveDestName = live.name;
        liveDestReady = true;
        liveRouteLabel.textContent = live.name;
        liveRoute.title = "Live · " + live.name;
      }
    }

    pendingRestores = Array.isArray(session.sources) ? session.sources.slice() : [];
    await tryRestorePendingSources();
  } catch (e) {
    console.warn("MixBridge session restore failed", e);
  } finally {
    restoringSession = false;
  }
}

function glyphForKind(kind) {
  if (kind === "process") return ICON.app;
  if (kind === "instrument") return ICON.keys;
  return ICON.mic;
}

function sourceDisplayName(src) {
  if (src.kind !== "physical" || !Number.isInteger(src.inputChannel)) return src.name;
  return `${src.name} · Input ${src.inputChannel + 1}`;
}

function renderSources() {
  sourceList.innerHTML = "";
  for (const src of sources.values()) {
    const card = document.createElement("div");
    card.className = "source-card";
    card.setAttribute("role", "listitem");
    card.dataset.id = String(src.id);

    card.innerHTML = `
      <div class="glyph">${glyphForKind(src.kind)}</div>
      <div class="identity" title="${sourceDisplayName(src)}">${sourceDisplayName(src)}</div>
      <div class="meter-wrap" aria-hidden="true"><div class="meter source-meter" data-meter="${src.id}"></div></div>
      <div class="source-actions">
        <button type="button" class="icon-btn tiny btn-mute ${src.mute ? "danger active" : ""}" title="Mute" aria-label="Mute" aria-pressed="${src.mute}">${ICON.mute}</button>
        ${src.kind === "instrument" ? `<button type="button" class="icon-btn tiny btn-keys active" title="Play" aria-label="Open instrument">${ICON.keys}</button>` : ""}
        <button type="button" class="icon-btn tiny btn-fx ${src.effectPath ? "active" : ""} ${src.effectFaulted ? "danger" : ""}" title="${src.effectName || "Effects"}" aria-label="Effects" aria-pressed="${!!src.effectPath}">${ICON.fx}</button>
        <input type="range" min="0" max="200" value="${Math.round(src.gain * 100)}" title="Level" aria-label="Level" class="gain" />
        <button type="button" class="icon-btn tiny btn-mon ${src.monitor ? "active" : ""}" title="Monitor" aria-label="Monitor" aria-pressed="${src.monitor}">${ICON.headphones}</button>
        <button type="button" class="icon-btn tiny btn-bc ${src.broadcast ? "active" : ""}" title="Live route" aria-label="Live route" aria-pressed="${src.broadcast}">${ICON.broadcast}</button>
        <button type="button" class="icon-btn tiny ghost btn-trash" title="Remove" aria-label="Remove">${ICON.trash}</button>
      </div>
    `;

    const keysButton = card.querySelector(".btn-keys");
    if (keysButton) {
      keysButton.addEventListener("click", () => openInstrument(src));
    }

    card.querySelector(".btn-fx").addEventListener("click", async () => {
      await showEffectList(src);
      if (!picker.open) picker.showModal();
    });

    card.querySelector(".btn-mute").addEventListener("click", async () => {
      const next = !src.mute;
      try {
        await invoke("engine_set_mute", { id: src.id, mute: next });
        src.mute = next;
        renderSources();
        scheduleSessionSave();
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".gain").addEventListener("input", async (ev) => {
      const gain = Number(ev.target.value) / 100;
      src.gain = gain;
      try {
        await invoke("engine_set_gain", { id: src.id, gain });
        scheduleSessionSave();
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".btn-mon").addEventListener("click", async () => {
      const next = !src.monitor;
      try {
        await invoke("engine_set_monitor", { id: src.id, enabled: next });
        src.monitor = next;
        renderSources();
        scheduleSessionSave();
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".btn-bc").addEventListener("click", async () => {
      const next = !src.broadcast;
      try {
        await invoke("engine_set_broadcast", { id: src.id, enabled: next });
        src.broadcast = next;
        renderSources();
        scheduleSessionSave();
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".btn-trash").addEventListener("click", async () => {
      try {
        await invoke("engine_remove_source", { id: src.id });
        sources.delete(src.id);
        renderSources();
        scheduleSessionSave();
        showError("");
      } catch (e) {
        showError(String(e));
      }
    });

    sourceList.appendChild(card);
  }
}

function upsertSource(dto) {
  const previous = sources.get(dto.id) || {};
  sources.set(dto.id, {
    ...previous,
    id: dto.id,
    kind: dto.kind,
    name: dto.name,
    mute: !!dto.mute,
    monitor: dto.monitor !== false,
    broadcast: dto.broadcast !== false,
    gain: typeof dto.gain === "number" ? dto.gain : 1,
    deviceId: dto.deviceId ?? previous.deviceId,
    inputChannel: dto.input_channel ?? dto.inputChannel ?? previous.inputChannel ?? null,
    processName: dto.processName ?? previous.processName,
    instrumentPreset: dto.instrument_preset ?? dto.instrumentPreset ?? previous.instrumentPreset ?? 0,
    effectName: dto.effect_name ?? dto.effectName ?? previous.effectName ?? "",
    effectPath: dto.effect_path ?? dto.effectPath ?? previous.effectPath ?? "",
    effectBypass: dto.effect_bypass ?? dto.effectBypass ?? previous.effectBypass ?? false,
    effectFaulted: dto.effect_faulted ?? dto.effectFaulted ?? previous.effectFaulted ?? false,
    effectEditorOpen: dto.effect_editor_open ?? dto.effectEditorOpen ?? previous.effectEditorOpen ?? false,
    effectDirty: dto.effect_dirty ?? dto.effectDirty ?? previous.effectDirty ?? false,
    effectStateFile: dto.effectStateFile ?? previous.effectStateFile ?? "",
  });
}

async function syncSourcesFromEngine() {
  const list = await invoke("engine_list_sources");
  const seen = new Set();
  for (const s of list) {
    seen.add(s.id);
    upsertSource(s);
  }
  for (const id of [...sources.keys()]) {
    if (!seen.has(id)) sources.delete(id);
  }
  renderSources();
}

function closePicker() {
  if (picker.open) picker.close();
  btnAdd.setAttribute("aria-expanded", "false");
}

function openPickerRoot() {
  pickerRoot.className = "picker-choices";
  pickerRoot.innerHTML = `
    <button type="button" class="choice" data-kind="physical" title="Input" aria-label="Physical input">
      ${ICON.mic}
      <span>Input</span>
    </button>
    <button type="button" class="choice" data-kind="process" title="Application" aria-label="Application">
      ${ICON.app}
      <span>Application</span>
    </button>
    <button type="button" class="choice" data-kind="instrument" title="Instrument" aria-label="Starter instrument">
      ${ICON.keys}
      <span>Instrument</span>
    </button>
  `;
  pickerRoot.querySelector('[data-kind="physical"]').addEventListener("click", showPhysicalList);
  pickerRoot.querySelector('[data-kind="process"]').addEventListener("click", showProcessList);
  pickerRoot.querySelector('[data-kind="instrument"]').addEventListener("click", showInstrumentPresets);
}

let currentInstrument = null;
const pointerNotes = new Map();
const keyboardNotes = new Map();
const computerNoteMap = new Map([
  ["a",60],["w",61],["s",62],["e",63],["d",64],["f",65],["t",66],
  ["g",67],["y",68],["h",69],["u",70],["j",71],["k",72],
]);

async function instrumentNoteOn(note, velocity = 0.82) {
  if (!currentInstrument) return;
  await invoke("engine_instrument_note_on", {
    id: currentInstrument.id, note, velocity
  }).catch((e) => showError(String(e)));
}

async function instrumentNoteOff(note) {
  if (!currentInstrument) return;
  await invoke("engine_instrument_note_off", {
    id: currentInstrument.id, note
  }).catch((e) => showError(String(e)));
}

async function instrumentAllNotesOff() {
  if (!currentInstrument) return;
  pointerNotes.clear();
  keyboardNotes.clear();
  await invoke("engine_instrument_notes_off", { id: currentInstrument.id }).catch(() => {});
  instrumentKeys.querySelectorAll(".piano-key.active").forEach((el) => el.classList.remove("active"));
}

function makePianoKey(note, label, black = false, left = null) {
  const key = document.createElement("button");
  key.type = "button";
  key.className = `piano-key ${black ? "black" : "white"}`;
  key.dataset.note = String(note);
  key.setAttribute("aria-label", label);
  key.title = label;
  if (left !== null) key.style.left = left;
  key.innerHTML = `<span>${label}</span>`;

  const down = async (ev) => {
    ev.preventDefault();
    key.setPointerCapture?.(ev.pointerId);
    pointerNotes.set(ev.pointerId, note);
    key.classList.add("active");
    await instrumentNoteOn(note);
  };
  const up = async (ev) => {
    const held = pointerNotes.get(ev.pointerId);
    if (held === undefined) return;
    pointerNotes.delete(ev.pointerId);
    key.classList.remove("active");
    await instrumentNoteOff(held);
  };
  key.addEventListener("pointerdown", down);
  key.addEventListener("pointerup", up);
  key.addEventListener("pointercancel", up);
  key.addEventListener("lostpointercapture", up);
  return key;
}

function renderInstrumentKeyboard() {
  instrumentKeys.innerHTML = "";
  const whites = [[60,"C"],[62,"D"],[64,"E"],[65,"F"],[67,"G"],[69,"A"],[71,"B"],[72,"C"]];
  const blacks = [[61,"C♯",9.3],[63,"D♯",21.8],[66,"F♯",46.8],[68,"G♯",59.3],[70,"A♯",71.8]];
  for (const [note,label] of whites) instrumentKeys.appendChild(makePianoKey(note,label));
  for (const [note,label,left] of blacks) instrumentKeys.appendChild(makePianoKey(note,label,true,`${left}%`));
}

async function openInstrument(src) {
  currentInstrument = src;
  instrumentTitle.textContent = src.name;
  renderInstrumentKeyboard();
  if (!instrumentDialog.open) instrumentDialog.showModal();
}

async function showInstrumentPresets() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", openPickerRoot);
  const presets = [
    { preset: 0, name: "Neon Keys" },
    { preset: 1, name: "Soft Pad" },
  ];
  for (const item of presets) {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "pick-row";
    row.title = item.name;
    row.setAttribute("aria-label", item.name);
    row.innerHTML = `${ICON.keys}<span>${item.name}</span>`;
    row.addEventListener("click", async () => {
      try {
        const res = await invoke("engine_add_instrument", { preset: item.preset });
        const src = {
          id: res.id,
          kind: "instrument",
          name: item.name,
          mute: false,
          monitor: true,
          broadcast: true,
          gain: 1,
          instrumentPreset: item.preset,
        };
        upsertSource(src);
        renderSources();
        scheduleSessionSave();
        closePicker();
        showError("");
        await openInstrument(sources.get(res.id));
      } catch (e) {
        showError(String(e));
      }
    });
    pickerRoot.appendChild(row);
  }
}

instrumentClose.addEventListener("click", async () => {
  await instrumentAllNotesOff();
  instrumentDialog.close();
  currentInstrument = null;
});
instrumentDialog.addEventListener("close", async () => {
  await instrumentAllNotesOff();
  currentInstrument = null;
});
instrumentDialog.addEventListener("click", async (ev) => {
  if (ev.target === instrumentDialog) {
    await instrumentAllNotesOff();
    instrumentDialog.close();
  }
});
window.addEventListener("blur", instrumentAllNotesOff);
window.addEventListener("keydown", async (ev) => {
  if (!instrumentDialog.open || ev.repeat) return;
  const note = computerNoteMap.get(ev.key.toLowerCase());
  if (note === undefined || keyboardNotes.has(ev.key.toLowerCase())) return;
  ev.preventDefault();
  keyboardNotes.set(ev.key.toLowerCase(), note);
  instrumentKeys.querySelector(`[data-note="${note}"]`)?.classList.add("active");
  await instrumentNoteOn(note);
});
window.addEventListener("keyup", async (ev) => {
  const key = ev.key.toLowerCase();
  const note = keyboardNotes.get(key);
  if (note === undefined) return;
  ev.preventDefault();
  keyboardNotes.delete(key);
  instrumentKeys.querySelector(`[data-note="${note}"]`)?.classList.remove("active");
  await instrumentNoteOff(note);
});

async function addPhysicalSource(device, inputChannel = null) {
  try {
    const res = await invoke("engine_add_physical", {
      deviceId: device.id,
      inputChannel,
    });
    upsertSource({
      id: res.id,
      kind: "physical",
      name: device.name,
      mute: false,
      monitor: true,
      broadcast: true,
      gain: 1,
      deviceId: device.id,
      inputChannel,
    });
    renderSources();
    scheduleSessionSave();
    closePicker();
    showError("");
  } catch (e) {
    showError(String(e));
  }
}

function showPhysicalChannelList(device) {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", showPhysicalList);

  const choices = [
    { channel: null, label: "Full input / stereo pair" },
    { channel: 0, label: "Input 1 · mono" },
    { channel: 1, label: "Input 2 · mono" },
  ];
  for (const choice of choices) {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "pick-row";
    row.title = `${device.name} · ${choice.label}`;
    row.setAttribute("aria-label", row.title);
    row.innerHTML = `${ICON.mic}<span>${choice.label}</span>`;
    row.addEventListener("click", () => addPhysicalSource(device, choice.channel));
    pickerRoot.appendChild(row);
  }
}

async function showPhysicalList() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", openPickerRoot);
  try {
    const devices = await invoke("engine_list_capture");
    for (const d of devices) {
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = d.name;
      row.setAttribute("aria-label", d.name);
      row.innerHTML = `${ICON.mic}<span>${d.name}</span>`;
      row.addEventListener("click", () => showPhysicalChannelList(d));
      pickerRoot.appendChild(row);
    }
  } catch (e) {
    showError(String(e));
  }
}

async function showProcessList() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", openPickerRoot);
  try {
    const procs = await invoke("engine_list_processes");
    for (const p of procs) {
      const label = p.name.replace(/\.exe$/i, "");
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = label;
      row.setAttribute("aria-label", label);
      row.innerHTML = `${ICON.app}<span>${label}</span>`;
      row.addEventListener("click", async () => {
        try {
          const res = await invoke("engine_add_process", { pid: p.pid, name: label });
          upsertSource({
            id: res.id,
            kind: "process",
            name: label,
            mute: false,
            monitor: true,
            broadcast: true,
            gain: 1,
            processName: label,
          });
          renderSources();
          scheduleSessionSave();
          closePicker();
          showError("");
        } catch (e) {
          showError(String(e));
        }
      });
      pickerRoot.appendChild(row);
    }
  } catch (e) {
    showError(String(e));
  }
}

async function showEffectList(src) {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", closePicker);

  if (src.effectPath) {
    const current = document.createElement("div");
    current.className = `fx-current ${src.effectFaulted ? "faulted" : ""}`;
    current.innerHTML = `
      <div class="fx-current-name">${ICON.fx}<span>${src.effectName || pluginNameFromPath(src.effectPath)}</span></div>
      <div class="fx-current-actions">
        <button type="button" class="icon-btn tiny fx-open active" title="Open effect" aria-label="Open effect editor">${ICON.window}</button>
        <button type="button" class="icon-btn tiny fx-bypass ${src.effectBypass ? "" : "active"}" title="Bypass" aria-label="Bypass effect" aria-pressed="${!!src.effectBypass}">${ICON.power}</button>
        <button type="button" class="icon-btn tiny ghost fx-clear" title="Remove effect" aria-label="Remove effect">${ICON.trash}</button>
      </div>`;
    current.querySelector(".fx-open").addEventListener("click", async () => {
      try {
        await invoke("engine_open_vst3_editor", { id: src.id });
        closePicker();
        showError("");
      } catch (e) {
        showError(String(e));
      }
    });
    current.querySelector(".fx-bypass").addEventListener("click", async () => {
      const next = !src.effectBypass;
      try {
        await invoke("engine_set_vst3_bypass", { id: src.id, bypass: next });
        src.effectBypass = next;
        renderSources();
        scheduleSessionSave();
        await showEffectList(src);
      } catch (e) {
        showError(String(e));
      }
    });
    current.querySelector(".fx-clear").addEventListener("click", async () => {
      try {
        await invoke("engine_clear_vst3", { id: src.id });
        src.effectName = "";
        src.effectPath = "";
        src.effectBypass = false;
        src.effectFaulted = false;
        src.effectStateFile = "";
        renderSources();
        scheduleSessionSave();
        closePicker();
        showError("");
      } catch (e) {
        showError(String(e));
      }
    });
    pickerRoot.appendChild(current);
  }

  try {
    const plugins = await invoke("vst3_list", { refresh: false });
    for (const plugin of plugins) {
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = plugin.name;
      row.setAttribute("aria-label", plugin.name);
      row.innerHTML = `${ICON.fx}<span>${plugin.name}</span>`;
      if (plugin.path === src.effectPath) row.classList.add("selected");
      row.addEventListener("click", async () => {
        try {
          await invoke("engine_set_vst3", { id: src.id, modulePath: plugin.path });
          src.effectName = plugin.name;
          src.effectPath = plugin.path;
          src.effectBypass = false;
          src.effectFaulted = false;
          src.effectStateFile = "";
          await captureEffectState(src);
          renderSources();
          scheduleSessionSave();
          closePicker();
          showError("");
        } catch (e) {
          showError(String(e));
        }
      });
      pickerRoot.appendChild(row);
    }
  } catch (e) {
    showError(String(e));
  }
}

async function showLiveOutputList() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", closePicker);
  try {
    const devices = await invoke("engine_list_render");
    for (const d of devices) {
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = d.name;
      row.setAttribute("aria-label", d.name);
      row.innerHTML = `${ICON.broadcast}<span>${d.name}</span>`;
      row.addEventListener("click", async () => {
        try {
          await invoke("engine_set_live_device", { deviceId: d.id });
          liveDestId = d.id;
          liveDestName = d.name;
          liveDestReady = true;
          liveRouteLabel.textContent = d.name;
          liveRoute.title = `Live · ${d.name}`;
          liveRoute.classList.remove("unavailable", "attention");
          scheduleSessionSave();
          closePicker();
          showError("");
          await refreshStatus();
        } catch (e) {
          showError(String(e));
        }
      });
      pickerRoot.appendChild(row);
    }
  } catch (e) {
    showError(String(e));
  }
}

async function showMonitorOutputList() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", closePicker);
  try {
    const devices = await invoke("engine_list_render");
    for (const d of devices) {
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = d.name;
      row.setAttribute("aria-label", d.name);
      row.innerHTML = `${ICON.headphones}<span>${d.name}</span>`;
      row.addEventListener("click", async () => {
        try {
          await invoke("engine_set_monitor_device", { deviceId: d.id });
          monitorDestId = d.id;
          monitorDestName = d.name;
          monitorRouteLabel.textContent = d.name;
          monitorRoute.title = `Monitor · ${d.name}`;
          monitorRoute.classList.add("selected");
          scheduleSessionSave();
          closePicker();
          showError("");
          await refreshStatus();
        } catch (e) {
          showError(String(e));
        }
      });
      pickerRoot.appendChild(row);
    }
  } catch (e) {
    showError(String(e));
  }
}

monitorRoute.addEventListener("click", () => {
  showMonitorOutputList();
  picker.showModal();
});
monitorRoute.addEventListener("keydown", (ev) => {
  if (ev.key === "Enter" || ev.key === " ") {
    ev.preventDefault();
    monitorRoute.click();
  }
});

liveRoute.addEventListener("click", () => {
  showLiveOutputList();
  picker.showModal();
});
liveRoute.addEventListener("keydown", (ev) => {
  if (ev.key === "Enter" || ev.key === " ") {
    ev.preventDefault();
    liveRoute.click();
  }
});

btnAdd.addEventListener("click", () => {
  openPickerRoot();
  picker.showModal();
  btnAdd.setAttribute("aria-expanded", "true");
});

picker.addEventListener("click", (ev) => {
  if (ev.target === picker) closePicker();
});

btnLive.addEventListener("click", async () => {
  try {
    if (onAir) {
      await invoke("broadcast_disable");
      setAirVisual(false);
      showError("");
      return;
    }
    // Ensure monitor engine stays running; Go Live is broadcast-only.
    await invoke("engine_ensure_running");
    if (!liveDestReady) {
      liveRoute.classList.add("attention");
      await showLiveOutputList();
      if (!picker.open) picker.showModal();
      setTimeout(() => liveRoute.classList.remove("attention"), 1800);
      return;
    }
    try {
      await invoke("broadcast_enable");
      setAirVisual(true);
      showError("");
    } catch (e) {
      // No live destination yet (Phase 6) — remain Standby, do not fake On Air.
      setAirVisual(false);
      const msg = String(e);
      if (msg.includes("no_live_destination")) {
        showError("");
      } else {
        showError(msg);
      }
    }
  } catch (e) {
    showError(String(e));
  }
});

let effectLifecycleBusy = false;
async function refreshEffectLifecycle() {
  if (effectLifecycleBusy) return;
  effectLifecycleBusy = true;
  try {
    const list = await invoke("engine_list_sources");
    for (const dto of list) {
      const src = sources.get(dto.id);
      if (!src) continue;
      const wasDirty = !!src.effectDirty;
      const wasOpen = !!src.effectEditorOpen;
      upsertSource(dto);
      const current = sources.get(dto.id);
      if (current?.effectPath && current.effectDirty && !current.effectEditorOpen) {
        await captureEffectState(current);
        current.effectDirty = false;
        scheduleSessionSave();
      } else if (wasOpen !== !!current?.effectEditorOpen || wasDirty !== !!current?.effectDirty) {
        renderSources();
      }
    }
  } catch {
    // Status loop owns visible engine errors; this maintenance pass stays quiet.
  } finally {
    effectLifecycleBusy = false;
  }
}

async function refreshStatus() {
  try {
    await invoke("engine_ensure_running");
    const s = await invoke("engine_status");
    engineOnline = true;
    liveDestReady = !!s.live_dest;
    // On Air only when broadcast is live AND a real destination exists.
    const live = liveDestReady && (s.broadcast || "").toLowerCase() === "live";
    setAirVisual(live);

    const m = await invoke("engine_meter");
    const pct = setMeterVertical(meterEl, m.peak);
    meterEl.setAttribute("aria-valuenow", String(pct));
    setMeterWidth(monitorMeterEl, m.peak);

    if (live) {
      try {
        const bm = await invoke("engine_meter_broadcast");
        setMeterWidth(liveMeterEl, bm.peak);
      } catch {
        setMeterWidth(liveMeterEl, 0);
      }
    } else {
      setMeterWidth(liveMeterEl, 0);
    }

    for (const src of sources.values()) {
      try {
        const sm = await invoke("engine_meter_source", { id: src.id });
        const el = sourceList.querySelector(`[data-meter="${src.id}"]`);
        if (el) setMeterWidth(el, sm.peak);
      } catch {
        /* ignore per-source meter misses */
      }
    }

    if (!statusEl.textContent.includes("no_live")) showError("");
  } catch (e) {
    engineOnline = false;
    setAirVisual(false);
    airBadge.textContent = "Offline";
    airBadge.classList.add("offline");
    airBadge.classList.remove("on-air");
    showError(String(e));
  }
}

(async () => {
  try {
    await invoke("engine_ensure_running");
    await syncSourcesFromEngine();
    await restoreLastSession();
  } catch (e) {
    showError(String(e));
  }
  setInterval(refreshStatus, 100);
  setInterval(refreshEffectLifecycle, 1000);
  setInterval(tryRestorePendingSources, 2000);
  refreshStatus();
})();
