# MixBridge QA — Phase 3 complete

Date: 2026-09-17T23:05:00Z

## Machine

| Field | Value |
|-------|-------|
| OS | Windows 11 25H2 build 26200 x64 |
| Audio | RME Fireface UC (capture 1+2 / render Speakers) |
| Engine format | 48 kHz float32 stereo |
| Observed device rate | 44100 Hz (shared WASAPI + autoconvert) |
| Buffer frames | 970 |
| Estimated latency | ~22 ms |
| WDK | not installed (driver packaging blocked; not a Phase 3 stop) |

## Phase 3 gates

| Gate | Description | Status | Evidence |
|------|-------------|--------|----------|
| 3.1 | Physical WASAPI source | **PASS** | `mb-engine-harness` physical_source on Fireface 1+2 |
| 3.2 | Render/monitor endpoint | **PASS** | Fireface Speakers; frames_rendered advancing |
| 3.3 | Process capture in engine | **PASS** | `mb-tone-player` → process_source; has1000=1 |
| 3.4 | Two simultaneous sources | **PASS** | physical + process + ref 440 mixed |
| 3.5 | Deterministic dual-tone analysis | **PASS** | tap has440=1 has1000=1; ctest mix_analyze |
| 3.6 | Gain verified | **PASS** | `engine_mix_analyze_test` ~0.5 ratio |
| 3.7 | Mute verified | **PASS** | mute A/B frequency disappearance |
| 3.8 | Meters non-blocking | **PASS** | AtomicMeter snapshots; IPC METER_MASTER |
| 3.9 | Device loss no crash | **PASS** | IMMNotificationClient → recovering/device_missing; no AV on physical AddRef fix |
| 3.10 | 30-minute soak | **PASS** | Maintainer accept; ticks xruns=0, WS ~11.8 MB; see `logs/soak-30min-ACCEPTANCE.md` |
| 3.11 | IPC start/stop/source | **PASS** | named pipe smoke: PING/ADD_TONE/START/METER/STOP |
| 3.12 | Tauri shell live meter | **PASS** | `prove-phase312-ipc.ps1` PASS; `npm run tauri -- dev` builds and runs `mixbridge-desktop.exe` |

## Commands

```text
cmake --build native/audio-engine/build
ctest --test-dir native/audio-engine/build --output-on-failure
native/audio-engine/build/mb-engine-harness.exe --seconds 2
native/audio-engine/build/mb-engine-soak.exe --minutes 2
powershell -File scripts/prove-phase312-ipc.ps1
cd apps/desktop; npm run tauri -- dev
```

## Phase 3 close

All twelve Phase 3 gates are **PASS**. Product-level 10/10 acceptance still requires later phases (VST3, virtual out, packaging).
