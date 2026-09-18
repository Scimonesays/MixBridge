# Session / preset foundation

Schema version 1. MixBridge now persists the last working setup locally as JSON under the Tauri application config directory.

The product default is a quiet auto-save/auto-restore flow: source/device choices and levels are remembered without adding helper copy to the main UI. Process sources are restored by application name when the application is available; unavailable applications remain pending and are retried while MixBridge is open.

```json
{
  "version": 1,
  "name": "Discord Jam",
  "monitor_device_id": "",
  "live_device_id": "",
  "sources": [
    {
      "kind": "physical",
      "device_id": "",
      "process_name": null,
      "name": "Fireface 1/2",
      "gain": 1.0,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": [],
      "fx_bypass": false,
      "fx_state_file": ""
    },
    {
      "kind": "process",
      "device_id": null,
      "process_name": "Chrome",
      "name": "Chrome",
      "gain": 0.8,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": []
    }
  ]
}
```

Current behavior:
- monitor and live destination IDs persist;
- physical sources restore by endpoint ID, with friendly-name fallback;
- application sources restore by process name and retry when the app starts later;
- gain, mute, monitor route, and broadcast route persist;
- the current VST3 insert path, bypass state, and full processor/controller snapshot persist;
- `fx` remains an array so later plugin-chain expansion does not require replacing the session shape.
