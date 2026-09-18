# MixBridge final acceptance — continuing

Date: 2026-09-18T00:30:00Z  
HEAD (local): see `git log -1`

## Score (honest)

Gates are tracked in `acceptance-matrix.json`.

| Area | Status |
|------|--------|
| Phase 3 engine | PASS |
| Phase 4.1 shell / Go Live semantics | PASS |
| Phase 5 Guitar Rig VST3 FX | PASS (`prove-phase5-fx`, `prove-dual-source-gr`) |
| Phase 6.1 Live WASAPI dest | PASS |
| Phase 6.2 MixBridge Output driver | **BLOCKED** — WDK headers present; `WindowsKernelModeDriver10.0` VS toolset still missing after quiet install attempt |
| NSIS installer | **Built** — `apps/desktop/src-tauri/target/release/bundle/nsis/MixBridge_0.1.0_x64-setup.exe` (~2.7 MB) |
| Discord E2E | **NOT RUN** — no virtual-cable capture endpoint on machine |
| Screenshots | Partial — `01-standby.png` captured; remaining doctrine shots pending |
| EV signing | External blocker |

## External blockers (exact)

1. **WindowsKernelModeDriver10.0** platform toolset not registered under VS 2022 BuildTools (only `v143` present). WDK 10.0.26100 is installed (`winget`), KM headers/`wdf.h` exist, but driver `.vcxproj` builds fail with MSB8020 until the VS ↔ WDK extension/toolset is installed (may require interactive VS Installer / reboot).
2. **No Discord-selectable MixBridge/VB-CABLE input** until (1) or an external virtual cable is installed.
3. **Production EV certificate** for shipping a signed kernel driver.

## Product path that works today

```text
Sources (physical / app / tone / FX incl. Guitar Rig 6)
  → Monitor (RME headphones)
  → Live (user-selected WASAPI render; e.g. virtual cable when present)
```

Discord Jam preset save/load is wired (`%LOCALAPPDATA%\MixBridge\presets`).

## Next actions (no stop)

1. Install VS WDK toolset interactively if quiet path keeps failing; rebuild `native/driver`.
2. Or install VB-CABLE → set as Live dest → Discord mic → prove L.
3. Finish remaining screenshots 02–07; polish UI from review.
4. Run NSIS install/uninstall smoke on a clean profile.
5. MIDI winmm input for instruments.
