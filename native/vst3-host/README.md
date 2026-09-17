# MixBridge VST3 host (Phase 5)

## Scope

Host VST3 effects/instruments inside MixBridge source FX chains.

## Status

Scaffold only. No SDK vendored yet.

## Plan

1. Pin Steinberg VST3 SDK (MIT) as a submodule or fetch script under `third_party/vst3sdk` with license verification.
2. Scan standard VST3 folders; cache results.
3. Load/unload/bypass/editor/state with quarantine for crashes.
4. Deterministic offline fixture tests before real Guitar Rig verification.

## Non-goals (license)

Do not copy GPL Vital/Helm sources into MixBridge. Research only.
