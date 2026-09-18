# MixBridge packaging (Phase 9)

## Goals

- MSI/NSIS or cargo-bundler installer
- Clean install / upgrade / uninstall
- Preserve `%LOCALAPPDATA%\MixBridge\presets`
- Ship `mb-engine-ipc.exe` + desktop shell + optional driver package

## Status

Tauri NSIS bundler is enabled (`apps/desktop/src-tauri/tauri.conf.json` → `bundle.targets: ["nsis"]`). Release desktop exe builds; full installer acceptance still in progress.

```text
cd apps/desktop
npm run tauri -- build
```

Artifacts typically land under `apps/desktop/src-tauri/target/release/` (exe) and `bundle/nsis/` (installer when bundling succeeds).

Driver package (`native/driver/Package`) is separate and requires the `WindowsKernelModeDriver10.0` toolset + signing.

## Checklist

| Check | How | Status |
|-------|-----|--------|
| Release exe launches | `mixbridge-desktop.exe` from `target/release` | PARTIAL — built on primary machine |
| NSIS installer produced | `npm run tauri -- build` with `bundle.active` | PARTIAL |
| Clean install | Install on clean/user profile; Start menu + icons | NOT RUN |
| Upgrade preserves presets | Save Discord Jam → upgrade → Load Discord Jam | NOT RUN |
| Uninstall leaves no services | Uninstall; confirm no engine/driver services remain | NOT RUN |
| Engine IPC shipped as resource | `mb-engine-ipc.exe` in bundle `resources` | Wired in config |
| Icons / Start menu | Branding under `apps/desktop/src-tauri/icons` | PARTIAL |
| Driver package (optional) | WDK build + test-sign / EV sign | BLOCKED (toolset / cert) |

## Notes

- Presets path must remain `%LOCALAPPDATA%\MixBridge\presets` across upgrades.
- Do not claim installer PASS until clean install / upgrade / uninstall are exercised and logged under `artifacts/qa/final/`.
