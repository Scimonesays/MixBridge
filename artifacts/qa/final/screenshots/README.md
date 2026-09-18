# Final acceptance screenshots (doctrine set)

Capture from a **working** MixBridge desktop build (release preferred). PNGs in this folder are gitignored; this checklist is tracked.

Helper: `powershell -File scripts/capture-screenshots.ps1` launches the release shell (or attaches if already running) and opens this folder.

## Required seven shots

| # | Filename | State to show |
|---|----------|---------------|
| 1 | `01-standby.png` | Branding, Sources → Mix → Outputs, real source cards, meters, monitor dest, calm **Standby** |
| 2 | `02-source-picker.png` | Clean `+` picker: physical input, application, working instruments — no helper paragraphs |
| 3 | `03-multi-source.png` | Guitar/input + app/backing track; independent meters, route icons, trash, gain |
| 4 | `04-vst-guitar.png` | Guitar source with FX (Guitar Rig or legal test VST) and active meter |
| 5 | `05-on-air.png` | Clear **On Air**, live + monitor destinations set, meters moving |
| 6 | `06-builtin-instrument.png` | Built-in instrument source; minimal controls (no giant synth panel) |
| 7 | `07-advanced-diag.png` | Polished Advanced / Diagnostics dialog only (gear) — keep tech off the main surface |

## Quality bar (gate Z)

Before marking `Y_screenshots` / `Z_screenshot_quality` PASS:

- Looks like a finished product, not scaffolding
- Consistent icons, spacing, card sizes; beautiful meters
- Eye reads left → right (Sources → Mix → Outputs) immediately
- On Air obvious; Standby calm; no unnecessary text; no accidental dead space
- Logo fits; cohesive neon broadcast identity

If any shot fails review: fix UI, retake, repeat.

## Capture tips

1. Prefer `apps/desktop/src-tauri/target/release/mixbridge-desktop.exe` (or installed NSIS build).
2. Use real hardware/apps when available (Fireface, Chrome, Guitar Rig 6).
3. For On Air: select a Live WASAPI render endpoint first (VB-CABLE or MixBridge Output when available).
4. Crop to the MixBridge window; avoid desktop clutter and debug overlays.
