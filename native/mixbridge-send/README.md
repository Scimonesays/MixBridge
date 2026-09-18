# MixBridge Send (VST3)

First-party VST3 insert effect: **stereo passthrough** plus a realtime copy of the host audio stream intended for MixBridge ingestion.

## Status (honest)

| Piece | Status |
|-------|--------|
| VST3 plugin build (`.vst3` bundle) | Scaffold — builds with Steinberg SDK helpers |
| Stereo in → stereo out passthrough | Implemented |
| Send Enable / Send Level parameters | Implemented |
| Realtime SHM ring (`Local\MixBridgeSend_<pid>_<instance>`) | Implemented |
| Pipe registration (`REGISTER_VST3_SEND`) | Stub ACK in `mb-engine-ipc` — **no engine source yet** |
| Engine send sink / `ADD_SEND_BUFFER` | **Not implemented** — future work |
| `mb-send-bridge` sidecar (SHM → UDP `127.0.0.1:47304`) | Scaffold for bring-up |

The plugin does **not** block the audio thread on named-pipe I/O. Registration runs on a control thread when the plugin activates; samples go to shared memory only.

## Prerequisites

- Windows x64
- CMake ≥ 3.24, Ninja (or VS generator)
- VST3 SDK fetched locally (same as `native/vst3-host`):

```powershell
powershell -File scripts/fetch-vst3sdk.ps1
```

SDK path: `third_party/vst3sdk/` (gitignored, MIT — see `docs/legal/LICENSE_MATRIX.md`).

## Build

```powershell
cmake -S native/mixbridge-send -B native/mixbridge-send/build -G Ninja
cmake --build native/mixbridge-send/build
```

Outputs (typical):

- `native/mixbridge-send/build/VST3/Release/MixBridge Send.vst3/` — install or symlink into your DAW scan folder
- `native/mixbridge-send/build/mb-send-bridge.exe` — optional sidecar

## Usage (scaffold)

1. Insert **MixBridge Send** on a stereo track in Ableton / REAPER / etc.
2. Set source label via env before launching the DAW (optional): `MIXBRIDGE_SEND_SOURCE_NAME=Ableton Main`
3. Start MixBridge engine IPC (`mb-engine-ipc`) — plugin sends `REGISTER_VST3_SEND` on activate (best effort).
4. Optional: run sidecar to forward SHM to UDP for debugging:

```powershell
# Mapping name is logged by the plugin / visible in Process Explorer handles, e.g.:
native/mixbridge-send/build/mb-send-bridge.exe --shm "Local\MixBridgeSend_12345_1"
```

## IPC protocol notes

**Control (named pipe `\\.\pipe\mixbridge-engine`):**

```
REGISTER_VST3_SEND <name> SHM Local\MixBridgeSend_<pid>_<instance>
=> OK RESERVED NAME ... NOTE send_sink_not_wired
```

**Audio (planned):** either engine pulls from SHM in-process, or `ADD_SEND_BUFFER` binary chunks on a side channel. Current scaffold uses SHM + optional UDP sidecar instead of pipe audio (pipe is line-oriented and unsuitable for realtime float blocks from the plugin RT thread).

## Architecture

```
DAW track → MixBridge Send.vst3
              ├─ passthrough → DAW output bus
              └─ interleaved float32 → SHM ring
                    └─ (optional) mb-send-bridge → UDP localhost:47304
                    └─ (future) MixBridge engine send sink → mix graph source
```

## Legal

- Plugin source: MixBridge (original, minimal SDK usage).
- Steinberg VST3 SDK: MIT, fetched separately — do not commit `third_party/vst3sdk/`.
