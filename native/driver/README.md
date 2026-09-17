# MixBridge virtual audio device (Phase 6)

## Goal

Expose a Windows capture endpoint named **MixBridge Output** that applications
(Discord, OBS, Zoom) can select as a microphone.

## Status

| Path | Status |
|------|--------|
| Broadcast bus in engine | Done |
| Live WASAPI render to selected endpoint | Phase 6.1 (user-selected render / virtual cable) |
| First-party MixBridge VAD (WDK) | Blocked until WDK + signing available |
| External virtual cable fallback | Supported via Live output picker (not bundled) |

## WDK blocker

This machine may not have the Windows Driver Kit installed. Absence of WDK must
not stop application development. Prefer:

1. Continue shipping the Live render-destination path (Phase 6.1)
2. Document production signing for a future `MixBridge Output` driver
3. Never claim Discord receives audio unless a real endpoint is selected

## Legal isolation

If adapting SysVAD (MS-PL) or MIT Virtual-Audio-Driver material, keep sources
under this tree with notices. Do not mix MS-PL into MIT-only libraries.

See `docs/legal/LICENSE_MATRIX.md` and `docs/research/UPSTREAM_REVIEW.md`.
