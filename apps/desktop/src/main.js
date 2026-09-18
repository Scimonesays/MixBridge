const airBadge = document.getElementById("air-badge");
const meterEl = document.getElementById("meter");
const monitorMeterEl = document.getElementById("monitor-meter");
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

/** @type {Map<number, {id:number, kind:string, name:string, deviceId:string, processName:string, mute:boolean, monitor:boolean, broadcast:boolean, gain:number, fx:string, fxPath:string, fxBypass:boolean, fxStatePath:string}>} */
const sources = new Map();

let onAir = false;
let liveDestReady = false;
let liveDestName = "";
let liveDeviceId = "";
let engineOnline = false;

const ICON = {
  mic: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 3a3 3 0 0 1 3 3v6a3 3 0 1 1-6 0V6a3 3 0 0 1 3-3zm-7 9a1 1 0 0 1 2 0 5 5 0 0 0 10 0 1 1 0 1 1 2 0 7 7 0 0 1-6 6.93V21h3a1 1 0 1 1 0 2H8a1 1 0 1 1 0-2h3v-2.07A7 7 0 0 1 5 12z"/></svg>`,
  app: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M4 4h7v7H4V4zm9 0h7v7h-7V4zM4 13h7v7H4v-7zm9 0h7v7h-7v-7z"/></svg>`,
  headphones: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 3a9 9 0 0 0-9 9v7a2 2 0 0 0 2 2h2v-8H5a7 7 0 0 1 14 0h-2v8h2a2 2 0 0 0 2-2v-7a9 9 0 0 0-9-9z"/></svg>`,
  broadcast: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 10a2 2 0 1 1 0 4 2 2 0 0 1 0-4zm-5.5-1.5a1 1 0 0 1 1.4 1.45 4 4 0 0 0 0 5.7 1 1 0 1 1-1.4 1.4 6 6 0 0 1 0-8.55zm11 0a6 6 0 0 1 0 8.55 1 1 0 1 1-1.4-1.4 4 4 0 0 0 0-5.7 1 1 0 0 1 1.4-1.45z"/></svg>`,
  mute: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M5 9v6h4l5 5V4L9 9H5zm12.5 3a4.5 4.5 0 0 0-2.5-4.03v8.06A4.5 4.5 0 0 0 17.5 12z"/></svg>`,
  trash: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M9 3h6l1 2h4v2H4V5h4l1-2zm1 6h2v10h-2V9zm4 0h2v10h-2V9zM7 9h2v10H7V9z"/></svg>`,
  plug: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M9 2h2v5H9V2zm4 0h2v5h-2V2zM7 9h10v3a5 5 0 0 1-4 4.9V21H9v-4.1A5 5 0 0 1 5 12V9h2z"/></svg>`,
  back: `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M14.7 5.3 8 12l6.7 6.7 1.4-1.4L10.8 12l5.3-5.3-1.4-1.4z"/></svg>`,
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
  el.style.height = `${pct}%`;
  el.style.width = "100%";
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

function glyphForKind(kind) {
  if (kind === "process") return ICON.app;
  if (kind === "tone") return ICON.plug;
  return ICON.mic;
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
      <div class="identity" title="${src.name}">${src.name}</div>
      ${src.fx ? `<div class="identity fx-label" title="${src.fx}">${src.fx}</div>` : ""}
      <div class="meter-wrap" aria-hidden="true"><div class="meter source-meter" data-meter="${src.id}"></div></div>
      <div class="source-actions">
        <button type="button" class="icon-btn tiny btn-mute ${src.mute ? "danger active" : ""}" title="Mute" aria-label="Mute" aria-pressed="${src.mute}">${ICON.mute}</button>
        <input type="range" min="0" max="200" value="${Math.round(src.gain * 100)}" title="Level" aria-label="Level" class="gain" />
        <button type="button" class="icon-btn tiny btn-mon ${src.monitor ? "active" : ""}" title="Monitor" aria-label="Monitor" aria-pressed="${src.monitor}">${ICON.headphones}</button>
        <button type="button" class="icon-btn tiny btn-bc ${src.broadcast ? "active" : ""}" title="Live route" aria-label="Live route" aria-pressed="${src.broadcast}">${ICON.broadcast}</button>
        <button type="button" class="icon-btn tiny btn-fx ${src.fx ? "active" : ""} ${src.fxBypass ? "danger" : ""}" title="Effects" aria-label="Effects">${ICON.plug}</button>
        <button type="button" class="icon-btn tiny ghost btn-trash" title="Remove" aria-label="Remove">${ICON.trash}</button>
      </div>
    `;

    card.querySelector(".btn-mute").addEventListener("click", async () => {
      const next = !src.mute;
      try {
        await invoke("engine_set_mute", { id: src.id, mute: next });
        src.mute = next;
        renderSources();
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".gain").addEventListener("input", async (ev) => {
      const gain = Number(ev.target.value) / 100;
      src.gain = gain;
      try {
        await invoke("engine_set_gain", { id: src.id, gain });
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
      } catch (e) {
        showError(String(e));
      }
    });
    card.querySelector(".btn-fx").addEventListener("click", async () => {
      if (src.fx) {
        await showFxMenu(src.id);
        picker.showModal();
      } else {
        await showFxList(src.id);
        picker.showModal();
      }
    });
    card.querySelector(".btn-trash").addEventListener("click", async () => {
      try {
        await invoke("engine_remove_source", { id: src.id });
        sources.delete(src.id);
        renderSources();
        showError("");
      } catch (e) {
        showError(String(e));
      }
    });

    sourceList.appendChild(card);
  }
}

async function showFxMenu(sourceId) {
  const src = sources.get(sourceId);
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `
    <button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>
    <button type="button" class="pick-row" data-act="editor" title="Editor" aria-label="Editor">${ICON.plug}<span>Editor</span></button>
    <button type="button" class="pick-row" data-act="bypass" title="Bypass" aria-label="Bypass">${ICON.mute}<span>${src?.fxBypass ? "Engage" : "Bypass"}</span></button>
    <button type="button" class="pick-row" data-act="replace" title="Replace" aria-label="Replace">${ICON.app}<span>Replace</span></button>
    <button type="button" class="pick-row" data-act="remove" title="Remove" aria-label="Remove">${ICON.trash}<span>Remove</span></button>
  `;
  pickerRoot.querySelector(".pick-back").addEventListener("click", closePicker);
  pickerRoot.querySelector('[data-act="editor"]').addEventListener("click", async () => {
    try {
      await invoke("engine_open_fx_editor", { id: sourceId });
      closePicker();
    } catch (e) {
      showError(String(e));
    }
  });
  pickerRoot.querySelector('[data-act="bypass"]').addEventListener("click", async () => {
    try {
      const next = !(src?.fxBypass);
      await invoke("engine_set_fx_bypass", { id: sourceId, bypass: next });
      if (src) src.fxBypass = next;
      renderSources();
      closePicker();
    } catch (e) {
      showError(String(e));
    }
  });
  pickerRoot.querySelector('[data-act="replace"]').addEventListener("click", async () => {
    await showFxList(sourceId, { replace: true });
  });
  pickerRoot.querySelector('[data-act="remove"]').addEventListener("click", async () => {
    try {
      await invoke("engine_remove_fx", { id: sourceId });
      if (src) {
        src.fx = "";
        src.fxPath = "";
        src.fxBypass = false;
        src.fxStatePath = "";
      }
      renderSources();
      closePicker();
    } catch (e) {
      showError(String(e));
    }
  });
}

async function showFxList(sourceId, opts = {}) {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", closePicker);
  try {
    const plugins = await invoke("engine_list_vst3");
    for (const p of plugins) {
      if (p.quarantine) continue;
      const row = document.createElement("button");
      row.type = "button";
      row.className = "pick-row";
      row.title = p.name;
      row.setAttribute("aria-label", p.name);
      row.innerHTML = `${ICON.plug}<span>${p.name}</span>`;
      row.addEventListener("click", async () => {
        try {
          if (opts.replace) await invoke("engine_remove_fx", { id: sourceId });
          await invoke("engine_add_fx", { id: sourceId, path: p.path });
          const src = sources.get(sourceId);
          if (src) {
            src.fx = p.name;
            src.fxPath = p.path;
            src.fxBypass = false;
          }
          renderSources();
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

function upsertSource(dto) {
  const prev = sources.get(dto.id);
  sources.set(dto.id, {
    id: dto.id,
    kind: dto.kind,
    name: dto.name,
    deviceId: dto.deviceId || dto.device_id || prev?.deviceId || "",
    processName: dto.processName || prev?.processName || "",
    mute: !!dto.mute,
    monitor: dto.monitor !== false,
    broadcast: dto.broadcast !== false,
    gain: typeof dto.gain === "number" ? dto.gain : 1,
    fx: dto.fx && dto.fx !== "-" ? dto.fx : prev?.fx || "",
    fxPath: dto.fxPath || prev?.fxPath || "",
    fxBypass: dto.fxBypass ?? prev?.fxBypass ?? false,
    fxStatePath: dto.fxStatePath || prev?.fxStatePath || "",
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
    <button type="button" class="choice" data-kind="instrument" title="Instrument" aria-label="Instrument">
      ${ICON.plug}
      <span>Instrument</span>
    </button>
  `;
  pickerRoot.querySelector('[data-kind="physical"]').addEventListener("click", showPhysicalList);
  pickerRoot.querySelector('[data-kind="process"]').addEventListener("click", showProcessList);
  pickerRoot.querySelector('[data-kind="instrument"]').addEventListener("click", showInstrumentList);
}

async function showInstrumentList() {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `<button type="button" class="icon-btn tiny pick-back" title="Back" aria-label="Back">${ICON.back}</button>`;
  pickerRoot.querySelector(".pick-back").addEventListener("click", openPickerRoot);
  const instruments = [
    { name: "Bass", hz: 110 },
    { name: "Keys", hz: 261.63 },
    { name: "Pad", hz: 220 },
    { name: "Click", hz: 880 },
  ];
  for (const inst of instruments) {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "pick-row";
    row.title = inst.name;
    row.setAttribute("aria-label", inst.name);
    row.innerHTML = `${ICON.plug}<span>${inst.name}</span>`;
    row.addEventListener("click", async () => {
      try {
        const res = await invoke("engine_add_tone", { hz: inst.hz });
        upsertSource({
          id: res.id,
          kind: "tone",
          name: inst.name,
          mute: false,
          monitor: true,
          broadcast: true,
          gain: 1,
          fx: "",
        });
        renderSources();
        closePicker();
        showError("");
      } catch (e) {
        showError(String(e));
      }
    });
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
      row.addEventListener("click", async () => {
        try {
          const res = await invoke("engine_add_physical", { deviceId: d.id });
          upsertSource({
            id: res.id,
            kind: "physical",
            name: d.name,
            deviceId: d.id,
            mute: false,
            monitor: true,
            broadcast: true,
            gain: 1,
          });
          renderSources();
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
            processName: p.name,
            mute: false,
            monitor: true,
            broadcast: true,
            gain: 1,
          });
          renderSources();
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
    const rank = (name) => {
      const n = (name || "").toLowerCase();
      if (n.includes("mixbridge")) return 0;
      if (n.includes("cable") || n.includes("vb-audio") || n.includes("voicemeeter")) return 1;
      if (n.includes("virtual")) return 2;
      return 3;
    };
    devices.sort((a, b) => rank(a.name) - rank(b.name) || a.name.localeCompare(b.name));
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
          liveDestName = d.name;
          liveDeviceId = d.id;
          liveDestReady = true;
          liveRoute.classList.remove("unavailable");
          liveRoute.classList.remove("needs-attention");
          closePicker();
          showError("");
          await refreshStatus();
          // Resume Go Live after destination is chosen.
          try {
            await invoke("broadcast_enable");
            setAirVisual(true);
          } catch {
            /* user can press Go Live again */
          }
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

document.getElementById("btn-preset").addEventListener("click", async () => {
  pickerRoot.className = "pick-list";
  pickerRoot.innerHTML = `
    <button type="button" class="pick-row" id="preset-save" title="Save Discord Jam" aria-label="Save Discord Jam">${ICON.broadcast}<span>Save Discord Jam</span></button>
    <button type="button" class="pick-row" id="preset-load" title="Load Discord Jam" aria-label="Load Discord Jam">${ICON.headphones}<span>Load Discord Jam</span></button>
  `;
  pickerRoot.querySelector("#preset-save").addEventListener("click", async () => {
    try {
      const { buildSessionSnapshot } = await import("./session.js");
      for (const s of sources.values()) {
        if (s.fx) {
          try {
            s.fxStatePath = await invoke("engine_save_fx_state", { id: s.id, index: 0 });
          } catch {
            /* optional */
          }
        }
      }
      const snap = buildSessionSnapshot({
        sources,
        liveDestName,
        liveDestReady,
        onAir,
        liveDeviceId,
      });
      await invoke("session_save", { name: "Discord Jam", json: JSON.stringify(snap, null, 2) });
      closePicker();
      showError("");
    } catch (e) {
      showError(String(e));
    }
  });
  pickerRoot.querySelector("#preset-load").addEventListener("click", async () => {
    try {
      const t0 = performance.now();
      const raw = await invoke("session_load", { name: "Discord Jam" });
      const snap = JSON.parse(raw);
      await invoke("engine_ensure_running");
      const current = await invoke("engine_list_sources");
      for (const s of current) {
        await invoke("engine_remove_source", { id: s.id });
      }
      sources.clear();
      if (snap.live_device_id) {
        try {
          await invoke("engine_set_live_device", { deviceId: snap.live_device_id });
          liveDeviceId = snap.live_device_id;
          liveDestName = snap.live_dest_name || "Live";
          liveDestReady = true;
        } catch {
          liveDestReady = false;
        }
      }
      for (const s of snap.sources || []) {
        let res = null;
        if (s.kind === "process") {
          const procs = await invoke("engine_list_processes");
          const want = (s.process_name || s.name || "").toLowerCase();
          const hit = procs.find(
            (p) =>
              p.name.toLowerCase().includes(want) ||
              p.name.replace(/\.exe$/i, "").toLowerCase() === want.replace(/\.exe$/i, ""),
          );
          if (!hit) continue;
          res = await invoke("engine_add_process", {
            pid: hit.pid,
            name: hit.name.replace(/\.exe$/i, ""),
          });
        } else if (s.kind === "tone") {
          const hz = /click/i.test(s.name) ? 880 : /pad/i.test(s.name) ? 220 : 110;
          res = await invoke("engine_add_tone", { hz });
        } else {
          res = await invoke("engine_add_physical", { deviceId: s.device_id || "" });
        }
        if (!res) continue;
        await invoke("engine_set_gain", { id: res.id, gain: s.gain ?? 1 });
        await invoke("engine_set_mute", { id: res.id, mute: !!s.mute });
        await invoke("engine_set_monitor", { id: res.id, enabled: s.monitor !== false });
        await invoke("engine_set_broadcast", { id: res.id, enabled: s.broadcast !== false });
        const fxList = Array.isArray(s.fx) ? s.fx : s.fx ? [{ name: s.fx, path: "" }] : [];
        let fxName = "";
        let fxPath = "";
        let fxStatePath = "";
        for (const fx of fxList) {
          let path = fx.path || "";
          if (!path && fx.name) {
            const plugs = await invoke("engine_list_vst3");
            const hit = plugs.find((p) => p.name === fx.name || (p.path || "").includes(fx.name));
            if (hit) path = hit.path;
          }
          if (!path) continue;
          await invoke("engine_add_fx", { id: res.id, path });
          fxName = fx.name || path;
          fxPath = path;
          if (fx.state_path) {
            try {
              await invoke("engine_load_fx_state", { id: res.id, index: 0, path: fx.state_path });
              fxStatePath = fx.state_path;
            } catch {
              /* optional */
            }
          }
        }
        upsertSource({
          id: res.id,
          kind: s.kind,
          name: s.name,
          deviceId: s.device_id || "",
          processName: s.process_name || "",
          mute: !!s.mute,
          monitor: s.monitor !== false,
          broadcast: s.broadcast !== false,
          gain: s.gain ?? 1,
          fx: fxName,
          fxPath,
          fxStatePath,
        });
      }
      await syncSourcesFromEngine();
      const ms = Math.round(performance.now() - t0);
      console.info(`preset_restore_ms=${ms}`);
      closePicker();
      showError("");
      setAirVisual(false);
    } catch (e) {
      showError(String(e));
    }
  });
  picker.showModal();
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
    await invoke("engine_ensure_running");
    if (!liveDestReady) {
      setAirVisual(false);
      liveRoute.classList.add("needs-attention");
      showLiveOutputList();
      picker.showModal();
      liveRoute.focus();
      showError("");
      return;
    }
    try {
      await invoke("broadcast_enable");
      liveRoute.classList.remove("needs-attention");
      setAirVisual(true);
      showError("");
    } catch (e) {
      setAirVisual(false);
      const msg = String(e);
      if (msg.includes("no_live_destination")) {
        liveRoute.classList.add("needs-attention");
        showLiveOutputList();
        picker.showModal();
        showError("");
      } else {
        showError(msg);
      }
    }
  } catch (e) {
    showError(String(e));
  }
});

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
  } catch (e) {
    showError(String(e));
  }
  setInterval(refreshStatus, 100);
  refreshStatus();
})();
