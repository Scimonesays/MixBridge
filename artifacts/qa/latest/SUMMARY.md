# MixBridge QA — V1 release-candidate baseline

Date: 2026-09-18
Commit: `146acc2d`

## Automated Windows release gates

GitHub Actions run `35314698283` completed **PASS** end-to-end.

| Gate | Status |
|---|---|
| Probes / deterministic DSP | PASS |
| Realtime engine build | PASS |
| Engine tests | PASS |
| Starter instrument non-silence test | PASS |
| Pinned Steinberg VST3 SDK build | PASS |
| Deterministic VST3 fixture build | PASS |
| Real VST3 load + audio processing probe | PASS |
| Desktop frontend production build | PASS |
| Tauri Rust check | PASS |
| Windows NSIS installer build | PASS |
| Installer SHA-256 generation | PASS |
| Installer artifact upload | PASS |
| License/provenance files present | PASS |

## Previously measured realtime evidence

Phase 3 gates 3.1–3.12 remain **PASS**, including physical RME capture/render, process loopback, dual-source analysis, device invalidation safety, named-pipe IPC, Tauri live control, and the accepted 30-minute soak with 0 xruns.

## Product semantics

- **Standby** keeps the engine and monitor path running.
- **Go Live / On Air** controls only the broadcast path.
- Physical input, application capture, built-in instruments, monitor routing, Live routing, source gain/mute/remove, VST3 insert/editor/state restore, and automatic saved-session restore are implemented.
- The supported V1 Discord path uses a real selected Windows Live render endpoint, such as an installed virtual cable.

## Hardware acceptance still required

Cloud CI cannot certify the final physical chain:

```text
RME / guitar → MixBridge → Guitar Rig VST3
Chrome / YouTube → MixBridge
MixBridge monitor → RME / headphones
MixBridge Live → installed virtual endpoint → Discord input
```

That real-machine smoke test must verify processed guitar + backing track, stable meters, no feedback loop, and correct Standby/On Air behavior.

## First-party MixBridge Output driver

**Not claimed as complete.** A branded signed `MixBridge Output` capture endpoint still requires WDK integration, real user-mode → kernel audio transport, production signing, installer lifecycle proof, and signed Discord/OBS capture validation. See `native/driver/README.md`.
