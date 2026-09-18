# MixBridge packaging (Phase 9 scaffold)

## Goals

- MSI/NSIS or cargo-bundler installer
- Clean install / upgrade / uninstall
- Preserve `LOCALAPPDATA\MixBridge\presets`
- Ship `mb-engine-ipc.exe` + desktop shell + optional driver package

## Status

Scaffold only. Tauri bundler config lives under `apps/desktop/src-tauri`.

```text
cd apps/desktop
npm run tauri -- build
```

Driver package (`native/driver/Package`) is separate and requires WDK toolset.

## Acceptance (not yet green)

| Check | Status |
|-------|--------|
| Clean install | NOT RUN |
| Upgrade preserves presets | NOT RUN |
| Uninstall leaves no services | NOT RUN |
| Icons / Start menu | PARTIAL (branding assets present) |
