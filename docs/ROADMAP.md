# MixBridge roadmap

## Phase 0 — Environment
Detect/install toolchain; document setup. **In progress / largely done on primary dev machine.**

## Phase 1 — Research
Upstream clones, license firewall, architecture selection. **In progress.**

## Phase 2 — Audio probes
Enumerate, capture, render, system loopback, process loopback, deterministic DSP tests. **Done on this machine.**

## Phase 3 — Engine
Realtime C++ engine with WASAPI sources/sinks, mix graph, meters, states, IPC, soak harness, Tauri shell. **Complete — gates 3.1–3.12 PASS** (`artifacts/qa/latest/SUMMARY.md`).

## Phase 4 — UI shell
Icon-first product shell: Sources → Mix → Outputs; engine vs broadcast state separated; real source picker (Input / Application); multi-source cards. **Phase 4.1 complete** — On Air awaits Phase 6 live destination.

## Phase 5 — VST3
Filesystem scanner (`mb-vst3-scan`) done. Full load/editor/state awaits pinned VST3 SDK.

## Phase 5 — VST3
**Hosting core complete** — Guitar Rig 6 proven (`prove-phase5-fx.ps1`, `prove-dual-source-gr.ps1`): load/process/bypass/state/chain/reorder. Editor API present.

## Phase 6 — Virtual output
- **6.1** Live WASAPI render destination (user-selected endpoint / virtual cable) — **done**; enables real On Air.
- **6.2** First-party MixBridge Output VAD — **scaffolded** in `native/driver/` (PortCls + KMDF); compile blocked on `WindowsKernelModeDriver10.0` toolset; install blocked on test/production signing until built.
- **Live path today:** WASAPI render to user-selected endpoint (6.1) until driver installed — see `docs/AUDIO_PIPELINE.md`.

## Phase 7 — Product UX
Discord Jam preset save/load wired; Instruments + FX in picker; Go Live focuses Live destination. Continue polish + screenshots.

## Phase 8 — Reliability
Hotplug, recovery, soak, feedback detection.

## Phase 9 — Packaging
Installer/upgrade/uninstall; driver signing story.

## Phase 10 — Final acceptance
All ten gates PASS with QA evidence.
