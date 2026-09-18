# MixBridge VST3 host (Phase 5)

## Scope

Host VST3 effects/instruments inside MixBridge source FX chains.

## Status

| Piece | Status |
|-------|--------|
| Filesystem `.vst3` scan (`mb-vst3-scan`) | Done — 33 plugins found on this machine including **Guitar Rig 6** |
| Steinberg VST3 SDK | Fetched locally via `scripts/fetch-vst3sdk.ps1` (not committed) |
| Process core (`mixbridge_vst3_host`) | Implemented on release branch — module/provider + float32 realtime blocks + bypass |
| Probe (`mb-vst3-probe`) | Implemented; accepts an installed `.vst3` path for real processing proof |
| Editor / parameter state / quarantine | Next |
| Engine FX insert | Next — wire per-source chain after host CI is green |

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
