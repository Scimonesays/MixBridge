# MixBridge architecture

Date: 2026-09-17  
Status: Selected after Phase 1 spike (research + environment + upstream review)

## Goals that drive architecture

1. Audio correctness and stability over UI cleverness
2. UI crash must not destroy Windows audio configuration
3. UI thread never processes audio; audio thread never waits on UI
4. Process/application loopback is a first-class source
5. VST3 hosting and virtual microphone output are core, not demos
6. Extremely simple UX over a sophisticated graph backend

## Options compared

### Option A — Monolithic Rust (cpal) + Tauri UI

```text
Tauri UI ──IPC──► Rust audio threads (cpal/WASAPI)
                      ├── capture
                      ├── mix
                      └── render / virtual cable
```

Pros:
- Faster initial scaffolding
- Single language across controller and much of I/O
- cpal is actively maintained (Apache-2.0)

Cons:
- Harder to guarantee dedicated process isolation if UI/host process dies
- Windows-specific low-latency / process-loopback / MMCSS details are easier to get wrong behind abstractions
- VST3 hosting ecosystem is stronger in native C++ (official SDK)

Verdict: Good for tools/probes; insufficient as the sole production realtime engine.

### Option B — Tauri UI + dedicated native C++20 WASAPI engine process (selected)

```text
┌──────────────────────────────┐
│       MixBridge UI           │
│ Tauri 2 + minimal web UI     │
└───────────────┬──────────────┘
                │ IPC (versioned)
┌───────────────▼──────────────┐
│     MixBridge Controller     │
│ config / devices / graph     │
│ (Rust crates in-process)     │
└───────────────┬──────────────┘
                │ control channel + shared rings
┌───────────────▼──────────────┐
│ Native Realtime Audio Engine │
│ C++20 / WASAPI (own process) │
│ float32 @ 48 kHz stereo bus  │
└───────┬──────────┬───────────┘
        │          │
     Inputs/DSP   VST3 host
        │
   Mix Graph (Monitor / Broadcast)
        │
   ┌────┴────┐
Monitor   Virtual Output backends
```

Pros:
- Matches Microsoft ApplicationLoopback sample language and WASAPI control surface
- Engine can outlive UI; heartbeat/watchdog can fail closed safely
- Aligns with proven MicDeck-style separation (studied under MIT; reimplemented, not vendored)
- Clear place for realtime rules (no alloc/IO/locks in callbacks)
- VST3 LIVE path can stay native; SAFE path can later isolate plugins

Cons:
- More build complexity (MSVC + CMake + Rust + Node)
- IPC and packaging must be versioned carefully

Verdict: Best fit for MixBridge priorities.

### Option C — Everything in-process native (no separate audio process)

Pros: slightly lower IPC overhead  
Cons: UI/host faults can take down audio and leave endpoint state harder to reason about  
Verdict: Rejected for production; acceptable only for early probes.

## Selection

**Selected: Option B.**

Reversible if measured evidence shows IPC overhead prevents instrument-monitoring latency targets on reference hardware. In that case, keep the same engine library but allow an in-process “LIVE embedded” mode for development machines only — still never mix UI work onto the audio thread.

## Canonical audio contract

| Parameter | Value |
|-----------|-------|
| Sample rate | 48,000 Hz internal |
| Sample format | 32-bit float interleaved |
| Master bus | stereo |
| Mono sources | upmix naturally |
| Resampling | only when device/plugin requires it |
| Realtime restrictions | no heap, filesystem, network, blocking locks, UI, logging I/O, sleep |

## Virtual output backends

1. **MixBridge VAD** (`native/driver/`) — goal production path (WDK + signing)
2. **Compatible external virtual cable** — optional fallback (e.g. user-installed VB-CABLE); never misrepresented as open source; not bundled until redistribution reviewed
3. **Test loopback backend** — CI / deterministic harness without hardware

Absence of a signed driver does **not** stop application development.

## Process loopback

Use Windows 10 build 20348+ application loopback (`ActivateAudioInterfaceAsync` + process tree params), following Microsoft’s ApplicationLoopback sample. Capture specific process trees (Chrome, Spotify, MixBridgeTestPlayer) without unrelated notification noise when a process is selected.

## VST3

- Official Steinberg VST3 SDK (MIT at pinned commit; re-verify on upgrade)
- LIVE: in-engine or tightly coupled native host for lowest latency
- SAFE: evaluated isolation (rust-vst3-host / out-of-process) when it works — no fake mode toggles

## Configuration

Human-readable versioned config with migration + backup before migrate (`crates/mixbridge-config`).

## IPC decision (Phase 3K)

Transport: **Windows named pipe** `\\.\pipe\mixbridge-engine`

Why:
- Simple to debug (line-oriented text protocol)
- Natural process isolation (UI/controller ≠ realtime)
- Adequate latency for control/metering (not sample transport)
- Sample audio stays inside the native engine process via WASAPI + SPSC rings

Binary: `mb-engine-ipc.exe`  
Protocol version header: `OK HELLO mixbridge-ipc/1`

Commands include `PING`, `STATUS` (engine + broadcast + LIVE_DEST), `START`, `STOP`, `RESTART`, `BROADCAST_ENABLE`, `BROADCAST_DISABLE`, `ADD_TONE`, `ADD_PHYSICAL`, `ADD_PROCESS`, `REMOVE`, `SET_GAIN`, `SET_MUTE`, `SET_MONITOR`, `SET_BROADCAST`, `METER_MASTER`, `METER_BROADCAST`, `METER_SOURCE`, `LIST_CAPTURE`, `LIST_RENDER`, `LIST_PROCESSES`, `LIST_SOURCES`, `SHUTDOWN`.

`BROADCAST_ENABLE` refuses with `no_live_destination` until a real live sink is registered (Phase 6). Dev-only override: `MIXBRIDGE_DEV_LIVE_SINK=1` (never a production destination).

Tauri shell talks to this pipe via Rust commands; it does not link the C++ engine in-process.

