# Session / preset schema (v1)

Schema version 1. Persistence: local JSON under `%LOCALAPPDATA%\MixBridge\presets\`.

## Status

**Implemented** — Discord Jam save/load is wired in the desktop shell (`apps/desktop/src/session.js` + Tauri `session_save` / `session_load`). UI: preset control → Save / Load Discord Jam.

## On-disk shape (example)

```json
{
  "version": 1,
  "name": "Discord Jam",
  "savedAt": "2026-09-18T00:00:00.000Z",
  "live_dest_ready": true,
  "live_dest_name": "CABLE Input",
  "live_device_id": "{wasapi-endpoint-id}",
  "on_air": false,
  "sources": [
    {
      "id": 1,
      "kind": "physical",
      "name": "Fireface 1/2",
      "device_id": "{capture-id}",
      "process_name": "",
      "gain": 1.0,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": [
        {
          "name": "Guitar Rig 6",
          "path": "C:\\Program Files\\Common Files\\VST3\\Guitar Rig 6.vst3",
          "state_path": "C:\\Users\\...\\AppData\\Local\\MixBridge\\fx-state\\..."
        }
      ]
    },
    {
      "id": 2,
      "kind": "process",
      "name": "chrome",
      "device_id": "",
      "process_name": "chrome",
      "gain": 0.8,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": []
    },
    {
      "id": 3,
      "kind": "tone",
      "name": "Keys",
      "device_id": "",
      "process_name": "",
      "gain": 1.0,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": []
    }
  ]
}
```

## Restore behavior

On load, the shell clears current engine sources, restores live device when `live_device_id` is set, re-adds sources by kind (`physical` / `process` / `tone`), reapplies gain/mute/monitor/broadcast, and reloads FX path + optional state blob when present. Process sources rematch by running process name (best-effort).
