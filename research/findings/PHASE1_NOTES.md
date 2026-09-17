# Phase 1 research notes

- Selected architecture: Tauri 2 + controller IPC + dedicated C++20 WASAPI engine process (see `docs/ARCHITECTURE.md`).
- Closest MIT reference product: MicDeck (Virtual-Soundboard) — adapt patterns, do not vendor tree.
- Process loopback authoritative sample: Microsoft ApplicationLoopback (MIT) — Initialize requires `AUDCLNT_STREAMFLAGS_LOOPBACK | EVENTCALLBACK | AUTOCONVERTPCM`.
- Virtual driver candidate: Virtual-Audio-Driver (MIT) + SysVAD (MS-PL isolate). WDK missing on current machine.
- Jamulus (AGPL) / SonoBus (GPL): reference only for future Jam mode.
- VST3 SDK MIT at pinned commit `3cdf9ca` — re-verify on upgrade.

Exact hashes: `research/findings/upstream-commits.json`
