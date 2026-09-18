// Session persistence helpers (loaded by main.js)

export function presetDirHint() {
  return "LOCALAPPDATA/MixBridge/presets";
}

export function buildSessionSnapshot({ sources, liveDestName, liveDestReady, onAir, liveDeviceId }) {
  return {
    version: 1,
    name: "Discord Jam",
    savedAt: new Date().toISOString(),
    live_dest_ready: !!liveDestReady,
    live_dest_name: liveDestName || "",
    live_device_id: liveDeviceId || "",
    on_air: !!onAir,
    sources: [...sources.values()].map((s) => ({
      id: s.id,
      kind: s.kind,
      name: s.name,
      device_id: s.deviceId || "",
      process_name: s.processName || "",
      gain: s.gain,
      mute: s.mute,
      monitor: s.monitor,
      broadcast: s.broadcast,
      fx: s.fxPath
        ? [{ name: s.fx || "", path: s.fxPath, state_path: s.fxStatePath || "" }]
        : s.fx
          ? [{ name: s.fx, path: "", state_path: "" }]
          : [],
    })),
  };
}
