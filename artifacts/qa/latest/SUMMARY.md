# MixBridge QA — continuing toward final acceptance

Date: 2026-09-18T00:10:00Z
Head: local `main` (post Phase 5 FX)

## Proven

| Area | Status | Evidence |
|------|--------|----------|
| Phase 3 gates | PASS | prior SUMMARY |
| Phase 4.1 semantics | PASS | prove-phase41-ipc |
| Go Live → Live picker when no dest | PASS | UI `needs-attention` + modal |
| VST3 load/process Guitar Rig 6 | PASS | mb-vst3-process + prove-phase5-fx |
| FX bypass/state/chain/reorder | PASS | prove-phase5-fx.ps1 |
| Dual physical+app + GR | see latest run | prove-dual-source-gr.ps1 |
| Discord Jam save/load | Wired | session.js + Tauri session_* |
| Live WASAPI destination | PASS | SET_LIVE_DEVICE |

## External / remaining blockers

| Item | Status |
|------|--------|
| `WindowsKernelModeDriver10.0` VS toolset | **BLOCKED** — WDK headers installed; Build Tools lack kernel toolset extension. Cannot compile MixBridge Output SYS yet. |
| Production EV driver signing | BLOCKED until certificate |
| Discord E2E manual | Pending once Live dest chosen (virtual cable or MixBridge Output) |
| Final screenshots / soak / installer | In progress |

## Workaround for Discord today

1. Install VB-CABLE (or similar) externally if desired.
2. MixBridge Live → select CABLE Input (render).
3. Discord Input → CABLE Output (capture).
4. Go Live.

First-party **MixBridge Output** replaces steps 1–3 when the WDK toolset is available.
