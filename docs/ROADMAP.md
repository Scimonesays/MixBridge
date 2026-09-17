# MixBridge roadmap

## Phase 0 — Environment
Detect/install toolchain; document setup. **In progress / largely done on primary dev machine.**

## Phase 1 — Research
Upstream clones, license firewall, architecture selection. **In progress.**

## Phase 2 — Audio probes
Enumerate, capture, render, system loopback, process loopback, deterministic DSP tests. **Done on this machine.**

## Phase 3 — Engine
Realtime C++ engine with WASAPI sources/sinks, mix graph, meters, states, IPC, soak harness, Tauri shell scaffold. **Nearly complete — see QA gates 3.1–3.12.**

## Phase 4 — UI shell
Polish Tauri icon-first product UI on top of working IPC.

## Phase 5 — VST3
Scan/cache/load/editor/bypass/state + quarantine.

## Phase 6 — Virtual output
MixBridge VAD + fallbacks; end-to-end virtual mic analysis.

## Phase 7 — Product UX
Icon-first home screen, source picker, presets, first-run.

## Phase 8 — Reliability
Hotplug, recovery, soak, feedback detection.

## Phase 9 — Packaging
Installer/upgrade/uninstall; driver signing story.

## Phase 10 — Final acceptance
All ten gates PASS with QA evidence.
