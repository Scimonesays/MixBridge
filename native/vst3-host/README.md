# MixBridge VST3 host (Phase 5)

## Scope

Host VST3 effects/instruments inside MixBridge source FX chains.

## Status

| Piece | Status |
|-------|--------|
| Filesystem `.vst3` scan (`mb-vst3-scan`) | Done — 33 plugins found on this machine including **Guitar Rig 6** |
| Steinberg VST3 SDK | Fetched locally via `scripts/fetch-vst3sdk.ps1` (not committed) |
| Process / load / editor / state | **Done** — `mb-vst3-process` + engine FX path |
| Engine FX insert | **Done** — `ADD_FX` / bypass / state / chain / reorder via IPC |
| Guitar Rig proof | **PASS** — `scripts/prove-phase5-fx.ps1`, `scripts/prove-dual-source-gr.ps1` (see `artifacts/qa/latest/logs/phase5-fx-proof.txt`, `dual-source-gr-proof.txt`) |

Hosting core is complete for product FX use. Remaining polish: richer editor UX, MIDI→VST3 `IEventList` (see `docs/MIDI.md`), quarantine UX.

## Commands

```text
powershell -File scripts/fetch-vst3sdk.ps1
cmake -S native/vst3-host -B native/vst3-host/build -G Ninja
cmake --build native/vst3-host/build
native/vst3-host/build/mb-vst3-scan.exe
powershell -File scripts/prove-phase5-fx.ps1
```

## Non-goals (license)

Do not copy GPL Vital/Helm sources into MixBridge. Research only.
