# MixBridge

**MixBridge makes complicated Windows audio routing feel almost invisible.**

Route a guitar or microphone, an application such as Chrome, and a VST3 effect into one simple live mix. Monitor it locally, then send that same mix to a real Windows Live output for Discord, OBS, meetings, games, or recording.

![MixBridge UI preview](docs/images/mixbridge-ui-preview.jpg)

> Rendered from the real MixBridge desktop shell with a representative saved session; this is not a separate concept mockup.

## V1 status

The **V1 application path is feature-complete on `finish/mixbridge-release`** and is held on that branch until its Windows CI/release gates are green.

Implemented today:

- physical WASAPI capture and per-application/process loopback
- realtime multi-source mixing, gain, mute, routing, meters and limiter
- selectable monitor and Live destinations with honest **Standby / On Air** state
- real VST3 discovery, loading and realtime processing
- per-source VST3 insert, bypass, dry fallback on fault, and saved component/controller state
- automatic last-session restoration of devices, applications, levels, routes and VST3 snapshots
- portable Tauri/NSIS packaging with the native audio engine bundled

### Live output today

V1 sends its broadcast mix to a **real Windows render endpoint selected in MixBridge**. For Discord microphone use today, select a compatible installed virtual-audio cable/loopback endpoint as Live output, then choose that cable's capture side in Discord.

A first-party signed endpoint named **MixBridge Output** is a separate Windows-driver deliverable requiring WDK work, a real user→kernel audio transport, signing and hardware acceptance. MixBridge does not fake that endpoint.

## Product flow

```text
Guitar / Mic ──► [ optional VST3, e.g. Guitar Rig ] ─┐
Chrome / App ─────────────────────────────────────────┼─► Live Mix ─┬─► Headphones / RME
Other source ─────────────────────────────────────────┘             └─► Live output ─► Discord
```

## Platform

- Windows 11 x64 — primary
- Windows 10 build 20348+ x64 — secondary

## Build the Windows installer

```powershell
powershell -File scripts/build-release.ps1
```

Output:

```text
apps\desktop\src-tauri\target\release\bundle\nsis
```

See `docs/SETUP.md` and `docs/TEST_PLAN.md` for toolchain and acceptance details.

## Architecture

Tauri 2 desktop UI + Rust controller commands + named-pipe IPC + dedicated C++20/WASAPI realtime engine + native VST3 hosting. See `docs/ARCHITECTURE.md`.

## Privacy

No telemetry, accounts, analytics, or cloud audio processing by default.

## License

MixBridge application code: MIT — see `LICENSE`. Third-party notices and the license firewall are documented under `docs/legal/` and `third_party/THIRD_PARTY_NOTICES.md`.

## Release discipline

Do not merge, publish, or release without explicit maintainer approval. V1 completion work is isolated on `finish/mixbridge-release`; `main` remains untouched until the maintainer chooses to merge it.
