# Session / preset foundation (Phase 7 start)

Schema version 1. Persistence target: local JSON under the user config directory.

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
      "name": "Fireface 1/2",
      "gain": 1.0,
      "mute": false,
      "monitor": true,
      "broadcast": true,
      "fx": []
    },
    {
      "kind": "process",
      "process_name": "chrome",
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

Not implemented in the UI yet — shape reserved so Phase 4 source state can map cleanly.
