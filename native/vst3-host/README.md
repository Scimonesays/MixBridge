# MixBridge VST3 host

## Scope

Host VST3 effects inside MixBridge source FX inserts with realtime-safe processing, native editor hosting, and state restore.

## Status

| Piece | Status |
|-------|--------|
| Filesystem `.vst3` scan (`mb-vst3-scan`) | Done — 33 plugins found on this machine including **Guitar Rig 6** |
| Steinberg VST3 SDK | Fetched locally via `scripts/fetch-vst3sdk.ps1` (not committed) |
| Process core (`mixbridge_vst3_host`) | Done — module/provider + 48 kHz float32 realtime blocks + bypass |
| Probe (`mb-vst3-probe`) | Done — deterministic fixture is loaded and audio-processed in CI |
| Native editor + parameter bridge | Done — plug-in editor hosted in a native Windows window |
| Processor/controller state snapshots | Done — saved/restored with MixBridge sessions |
| Engine FX insert | Done — one realtime insert per source with fault-to-dry fallback |
| Full plug-in process isolation | Future hardening — current faults fall back dry; host-process isolation is not claimed |

## Commands

```text
powershell -File scripts/fetch-vst3sdk.ps1
cmake -S native/vst3-host -B native/vst3-host/build -G Ninja
cmake --build native/vst3-host/build
native/vst3-host/build/mb-vst3-scan.exe
```

## Non-goals (license)

Do not copy GPL Vital/Helm sources into MixBridge. Research only.


## Processing probe

After fetching the pinned SDK:

```text
cmake -S native/vst3-host -B native/vst3-host/build
cmake --build native/vst3-host/build --config Release
native/vst3-host/build/Release/mb-vst3-probe.exe "C:\\Program Files\\Common Files\\VST3\\Guitar Rig 6.vst3"
```

The probe is intentionally generic. Guitar Rig is a real-world target, not a special-cased dependency.
