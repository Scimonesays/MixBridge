# MixBridge audio pipeline

Date: 2026-09-17 (Phase 3 measured)

## Canonical internal format

| Property | Value |
|----------|-------|
| Rate | 48,000 Hz |
| Sample format | float32 interleaved |
| Channels | stereo master (mono sources upmixed) |
| Buses | Monitor (wired to WASAPI render), Broadcast (mixed + safety limiter) |

## Live / broadcast output path

| Stage | Current (Phase 6.1) | Target (Phase 6 + driver) |
|-------|---------------------|---------------------------|
| Engine render sink | User-selected WASAPI **render** endpoint (virtual cable or physical device) | **MixBridge Input** (first-party virtual speaker) |
| App mic source (Discord, OBS) | External virtual cable loopback, if configured | **MixBridge Output** (first-party virtual mic) |

Until `native/driver/` is built, test-signed, and installed, **Live uses WASAPI render only**. Do not claim Discord receives audio unless a real endpoint is selected and verified.

## Realtime architecture (implemented)

```text
Capture threads (WASAPI event-driven)
  → SPSC float rings @ 48 kHz stereo
Render thread (monitor WASAPI event callback)
  → MixGraph::process (chunked, no heap)
  → AtomicMeter snapshots
  → WasapiRenderSink
```

Device notifications (`IMMNotificationClient`) move the engine into `recovering` / `device_missing` instead of crashing.

## Measured on this machine

| Item | Value |
|------|-------|
| Monitor device | RME Fireface UC Speakers |
| Device mix rate observed | 44,100 Hz (AUTOCONVERT from engine 48 kHz) |
| Shared buffer frames | 970 |
| Estimated latency | ~22 ms (buffer_frames / device_rate) |
| Dual-source harness | 440 Hz tone + process-loopback 1000 Hz → both detected in tap |
| Physical capture | RME Analog 1+2 integrated without crash (AddRef race fixed) |

## Conversion policy

When a device rejects float32/48k stereo, the engine falls back to the device mix format and converts to/from engine float stereo at the capture/render boundary (`AUTOCONVERTPCM` preferred when requesting engine format).

## Realtime prohibitions (enforced by design)

No heap / filesystem / network / UI / blocking control mutex / disk logging on the render callback path. Tap uses a lock-free SPSC ring.
