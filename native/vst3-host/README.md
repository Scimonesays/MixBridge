# MixBridge VST3 host (Phase 5)

## Scope

Host VST3 effects/instruments inside MixBridge source FX chains.

## Status

| Piece | Status |
|-------|--------|
| Filesystem `.vst3` scan (`mb-vst3-scan`) | Done — 33 plugins found on this machine including **Guitar Rig 6** |
| Steinberg VST3 SDK | Fetched locally via `scripts/fetch-vst3sdk.ps1` (not committed) |
| Process / load / editor / state | Next — build thin host on `public.sdk` hosting APIs |
| Engine FX insert | Not wired yet |

## Commands

```text
powershell -File scripts/fetch-vst3sdk.ps1
cmake -S native/vst3-host -B native/vst3-host/build -G Ninja
cmake --build native/vst3-host/build
native/vst3-host/build/mb-vst3-scan.exe
```

## Non-goals (license)

Do not copy GPL Vital/Helm sources into MixBridge. Research only.
