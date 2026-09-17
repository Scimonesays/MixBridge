# Local reference notes (WASAPI / mixer)

Date: 2026-09-17  
Scope: Phase 3 realtime engine. Patterns only — no bulk code transplant.

## Godot (`godot-master/drivers/wasapi`)

- Enumerates `DEVICE_STATE_ACTIVE` endpoints via `IMMDeviceEnumerator`.
- Prefers `IAudioClient3` for **output** shared-mode period control; falls back to `IAudioClient` on older OS / failure.
- Treats `AUDCLNT_STREAMFLAGS_RATEADJUST` as incompatible with Client3 init path.
- Shared-mode period frames must be integral multiples of fundamental period when using Client3.
- Clear verbose HRESULT logging on activate/init failure — MixBridge should map HRESULTs to engine states, not crash.

**Apply:** Shared WASAPI + event callback; try Client3 for monitor sink later; always keep Client1 fallback. Device loss → explicit recovering/failed state.

## Microsoft ApplicationLoopback (already in research/upstream)

- Process loopback: `ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, …)`.
- Initialize with `LOOPBACK | EVENTCALLBACK | AUTOCONVERTPCM` and an explicit PCM format.
- Proven in MixBridge probes (`dominant_hz=1000` with `mb-tone-player`).

**Apply:** Reuse this exact init contract in production `ProcessLoopbackSource`.

## RetroArch / Blender archives

- Zip present under `_archives/`; not fully expanded this pass.
- Cross-check later for exclusive-mode and recovery edge cases if shared-mode instrument latency is insufficient.

## SchismTracker / BambooTracker

- Schism routes audio via SDL backends (`wasapi` listed as SDL option) — useful as **backend abstraction** reminder, not a WASAPI implementation source.
- BambooTracker: ASIO concepts for a future isolated backend. **Do not add ASIO in Phase 3.**

## LMMS (archive)

- Bus / send / mixer graph ownership ideas for Monitor vs Broadcast separation.
- Do **not** adopt DAW timeline or track UI.

## MuseScore / Sonic Pi

- Reinforce UI↔audio process separation and non-realtime control messaging (feeds Phase 3K IPC).

## MixBridge decisions from comparison

| Topic | Decision |
|-------|----------|
| Share mode | Shared + event-driven first |
| Format | Request float32 / 48 kHz / stereo via AUTOCONVERTPCM; record actual mix format |
| Clock | Render (monitor) callback drives the mix |
| Capture | Side threads → SPSC rings → mix |
| Recovery | Explicit engine states; no silent half-running |
| ASIO | Deferred |
