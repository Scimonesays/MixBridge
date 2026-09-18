# MixBridge

**MixBridge makes complicated PC audio routing feel almost invisible.**

Route a guitar or microphone, an application such as Chrome, and a VST3 effect into one simple live mix. Monitor it locally, then send that same mix to a real Windows Live output for Discord, OBS, meetings, games, or recording.

![MixBridge UI preview](docs/images/mixbridge-ui-preview.jpg)

> UI preview rendered from the production MixBridge desktop shell with a representative saved session. Hardware acceptance screenshots remain separate evidence.

## V1 status

The **V1 application path is feature-complete and CI-proven on `main`**. The Windows release pipeline builds and tests the realtime engine, deterministic VST3 processing, desktop shell, Tauri backend, and NSIS installer.

Implemented today:

- physical WASAPI capture and per-application/process loopback
- realtime multi-source mixing, gain, mute, routing, meters and limiter
- selectable monitor and Live destinations with honest **Standby / On Air** state
- real VST3 discovery, loading and realtime processing, including the plug-in's native editor window
- per-source VST3 insert, bypass, dry fallback on fault, and saved component/controller state
- automatic last-session restoration of devices, applications, levels, routes and VST3 snapshots
- built-in **Neon Keys** and **Soft Pad** instruments from the + source picker, with on-screen/computer keyboard play
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

`main` is the release-candidate source of truth. Every change must preserve the green Windows CI gates, license firewall, honest hardware/driver status, and the Sources → Mix → Outputs product doctrine.
