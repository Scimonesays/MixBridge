# MixBridge

**MixBridge makes complicated PC audio routing feel almost invisible.**

Connect a guitar, microphone, browser, backing track, or VST3 effect. Select what you want. Send the mix to Discord, OBS, games, meetings, or a recorder — without learning virtual cables, buses, or Windows audio internals.

## Status

**Active development — not production-ready.** Acceptance gates are tracked in `docs/TEST_PLAN.md`. Do not treat a green compile or an open window as completion.

## Platform

- Primary: Windows 11 x64
- Secondary: Windows 10 build 20348+ x64

## Architecture (selected)

Tauri 2 UI + controller IPC + dedicated native C++20 WASAPI realtime audio process + isolated VST3 host + virtual output backends. See `docs/ARCHITECTURE.md`.

## Quick start (developers)

See `docs/SETUP.md` for toolchain requirements and first build.

High level:

```text
1. Install documented toolchain (MSVC, CMake, Rust, Node)
2. Build native probes: scripts/build-probes.ps1
3. Run automated audio tests: scripts/run-audio-tests.ps1
4. Build desktop shell when UI phase begins
```

## Repository layout

```text
apps/desktop/     Tauri 2 UI
native/           C++ audio engine, VST3 host, driver, probes
crates/           Rust IPC / model / config
tests/            unit, integration, audio, e2e, soak
tools/            device / latency / virtual-output probes
research/         findings (upstream clones are gitignored)
docs/             architecture, UX, legal, research
```

## Privacy

No telemetry, accounts, analytics, or cloud processing by default. Audio stays local unless you deliberately enable a future network feature.

## License

MixBridge application code: MIT — see `LICENSE`.

Third-party notices and license firewall: `docs/legal/LICENSE_MATRIX.md`, `third_party/THIRD_PARTY_NOTICES.md`.

## Contributing note

Do not push, publish, or release without explicit maintainer approval. Local development and commits on the default branch are the current workflow.
