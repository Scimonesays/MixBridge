# MixBridge test plan & acceptance gates

Status legend: PASS / FAIL / BLOCKED / NOT RUN / PARTIAL

## Proven realtime engine gates

See `artifacts/qa/latest/SUMMARY.md`.

| Gate | Status |
|---|---|
| Physical capture | PASS |
| Monitor render | PASS |
| Process loopback | PASS |
| Dual-source mix | PASS |
| Deterministic analysis | PASS |
| Gain / mute / routing / meters | PASS |
| Device-loss safety | PASS |
| 30-minute engine soak | PASS — accepted evidence, xruns=0 |
| Named-pipe IPC | PASS |
| Tauri live control/meter path | PASS |

## V1 automated release pipeline

The `finish/mixbridge-release` GitHub Actions run is authoritative for merge readiness.

| Gate | Required proof |
|---|---|
| Probes | configure/build + deterministic DSP tests |
| Engine | configure/build + engine tests |
| VST3 host | pinned SDK + host/scanner build |
| Real VST3 processing | build deterministic fixture, load it, process audio |
| Desktop | production frontend build |
| Tauri | `cargo check --locked` |
| Installer | `scripts/build-release.ps1` produces NSIS bundle |
| Legal | license/provenance files present |

## Product semantics already implemented

- engine state is independent from On Air state
- Go Live never fakes a missing destination
- physical + application source selection
- real monitor + Live output selection
- multi-source add/remove/gain/mute/route
- per-source VST3 insert/bypass/fault-to-dry
- VST3 component + controller state snapshots
- automatic source/device/process/VST restore
- portable bundled native engine sidecar
- first-party starter instrument source with deterministic non-silence engine test and session restore

## Final hardware smoke test before public release

A cloud runner cannot certify the physical chain. On the target Windows machine verify:

```text
RME/guitar → MixBridge → Guitar Rig VST3
Chrome/YouTube → MixBridge
MixBridge monitor → RME/headphones
MixBridge Live → installed virtual endpoint → Discord input
```

Confirm processed guitar + backing track, stable meters, no feedback loop, and correct Standby/On Air behavior.

## First-party MixBridge Output driver

**BLOCKED as a signed artifact**, not faked as PASS. It still requires WDK build, real driver audio injection, production signing and signed endpoint acceptance.
