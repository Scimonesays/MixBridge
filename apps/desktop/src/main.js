const airBadge = document.getElementById("air-badge");
const meterEl = document.getElementById("meter");
const sourceMeterEl = document.getElementById("source-meter");
const monitorMeterEl = document.getElementById("monitor-meter");
const liveMeterEl = document.getElementById("live-meter");
const liveRoute = document.getElementById("live-route");
const sourceCard = document.getElementById("source-card");
const statusEl = document.getElementById("status");
const btnLive = document.getElementById("btn-live");
const liveLabel = document.getElementById("live-label");
const btnMute = document.getElementById("btn-mute");

let sourceId = null;
let muted = false;
let onAir = false;

function showError(msg) {
  statusEl.textContent = msg || "";
}

function setMeter(el, peak, vertical) {
  const pct = Math.min(100, Math.round((peak || 0) * 140));
  if (vertical) {
    el.style.height = `${pct}%`;
    el.style.width = "100%";
  } else {
    el.style.width = `${pct}%`;
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
}

async function invoke(cmd, args = {}) {
  if (!window.__TAURI_INTERNALS__) {
    return Promise.reject(new Error("Tauri runtime required"));
  }
  const { invoke } = await import("@tauri-apps/api/core");
  return invoke(cmd, args);
}

async function refreshStatus() {
  try {
    const s = await invoke("engine_status");
    const running = (s.state || "").toLowerCase() === "running";
    setAirVisual(running);
    const m = await invoke("engine_meter");
    const peak = m.peak || 0;
    const pct = setMeter(meterEl, peak, true);
    meterEl.setAttribute("aria-valuenow", String(pct));
    setMeter(sourceMeterEl, peak, false);
    setMeter(monitorMeterEl, peak, false);
    setMeter(liveMeterEl, running ? peak : 0, false);
    showError("");
  } catch (e) {
    setAirVisual(false);
    airBadge.textContent = "Offline";
    airBadge.classList.add("offline");
    airBadge.classList.remove("on-air");
    showError(String(e));
  }
}

document.getElementById("btn-add-tone").addEventListener("click", async () => {
  try {
    const res = await invoke("engine_add_tone", { hz: 440 });
    sourceId = res.id;
    sourceCard.hidden = false;
    showError("");
  } catch (e) {
    showError(String(e));
  }
});

btnLive.addEventListener("click", async () => {
  try {
    if (onAir) {
      await invoke("engine_stop");
    } else {
      await invoke("engine_start");
    }
    await refreshStatus();
  } catch (e) {
    showError(String(e));
  }
});

btnMute.addEventListener("click", async () => {
  if (!sourceId) return;
  muted = !muted;
  try {
    await invoke("engine_set_mute", { id: sourceId, mute: muted });
    btnMute.classList.toggle("danger", muted);
    btnMute.classList.toggle("active", muted);
    btnMute.setAttribute("aria-pressed", muted ? "true" : "false");
  } catch (e) {
    showError(String(e));
  }
});

document.getElementById("gain").addEventListener("input", async (ev) => {
  if (!sourceId) return;
  const gain = Number(ev.target.value) / 100;
  try {
    await invoke("engine_set_gain", { id: sourceId, gain });
  } catch (e) {
    showError(String(e));
  }
});

setInterval(refreshStatus, 100);
refreshStatus();
