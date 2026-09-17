# MixBridge test plan & acceptance gates

Status legend: PASS / FAIL / BLOCKED / NOT RUN / PARTIAL

## Phase 3 realtime gates (current)

See `artifacts/qa/latest/SUMMARY.md` for measured evidence.

| Gate | Status |
|------|--------|
| 3.1 Physical source | PASS |
| 3.2 Monitor render | PASS |
| 3.3 Process loopback in engine | PASS |
| 3.4 Dual-source mix | PASS |
| 3.5 Deterministic analysis | PASS |
| 3.6 Gain | PASS |
| 3.7 Mute | PASS |
| 3.8 Meters | PASS |
| 3.9 Device loss safety | PASS |
| 3.10 30-min soak | PASS (maintainer accept; xruns=0, WS stable) |
| 3.11 IPC | PASS |
| 3.12 Tauri live meter | PASS (IPC live-control proof + Tauri `dev` run) |

## Product gates (overall)

| Gate | Description | Status |
|------|-------------|--------|
| 1 Clean Build | Probes + engine build | PASS (native) |
| 2 Clean Launch | App launches | PASS (Tauri `mixbridge-desktop` `dev`) |
| 3 Physical Audio | Product path | PARTIAL (engine harness; Phase 4 product shell next) |
| 4 Application Capture | Engine process loopback | PASS (harness) |
| 5–9 | VST3 / virtual out / UX / soak product | NOT RUN |
| 10 Repository Quality | Docs tracking reality | PASS for Phase 3 docs |

**10/10 product acceptance still requires all ten product gates.**
