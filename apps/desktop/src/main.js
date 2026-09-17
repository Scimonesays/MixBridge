const stateEl = document.getElementById("state");
const meterEl = document.getElementById("meter");
const statusEl = document.getElementById("status");

let sourceId = null;
let muted = false;

function showError(msg) {
  statusEl.hidden = !msg;
  statusEl.textContent = msg || "";
}

async function invoke(cmd, args = {}) {
  if (!window.__TAURI_INTERNALS__) {
    // Browser-only preview fallback
    return { ok: false, error: "Tauri runtime required" };
  }
  const { invoke } = await import("@tauri-apps/api/core");
  return invoke(cmd, args);
}

async function refreshStatus() {
  try {
    const s = await invoke("engine_status");
    stateEl.textContent = s.state || "—";
    const m = await invoke("engine_meter");
    const pct = Math.min(100, Math.round((m.peak || 0) * 100));
    meterEl.style.width = `${pct}%`;
    meterEl.setAttribute("aria-valuenow", String(pct));
    showError("");
  } catch (e) {
    stateEl.textContent = "offline";
    showError(String(e));
  }
}

document.getElementById("btn-add-tone").addEventListener("click", async () => {
  try {
    const res = await invoke("engine_add_tone", { hz: 440 });
    sourceId = res.id;
  } catch (e) {
    showError(String(e));
  }
});

document.getElementById("btn-start").addEventListener("click", async () => {
  try {
    await invoke("engine_start");
    await refreshStatus();
  } catch (e) {
    showError(String(e));
  }
});

document.getElementById("btn-stop").addEventListener("click", async () => {
  try {
    await invoke("engine_stop");
    await refreshStatus();
  } catch (e) {
    showError(String(e));
  }
});

document.getElementById("btn-mute").addEventListener("click", async () => {
  if (!sourceId) return;
  muted = !muted;
  try {
    await invoke("engine_set_mute", { id: sourceId, mute: muted });
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
