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
Scan/cache/load/editor/bypass/state + quarantine. **Next.**

## Phase 6 — Virtual output
- **6.1** Live WASAPI render destination (user-selected endpoint / virtual cable) — **done**; enables real On Air.
- **6.2** First-party MixBridge Output VAD — **blocked on WDK/signing**; scaffold in `native/driver/`.

## Phase 7 — Product UX
Icon-first home screen, source picker, presets, first-run.

## Phase 8 — Reliability
Hotplug, recovery, soak, feedback detection.

## Phase 9 — Packaging
Installer/upgrade/uninstall; driver signing story.

## Phase 10 — Final acceptance
All ten gates PASS with QA evidence.
