# MixBridge audio pipeline

Canonical internal format:

| Property | Value |
|----------|-------|
| Rate | 48,000 Hz |
| Format | IEEE float32 interleaved |
| Buses | Monitor, Broadcast (extensible) |
| Master | stereo |

## Source types (V1)

1. Physical capture endpoints (instrument / mic)
2. System loopback (entire render device mix)
3. Process loopback (process tree via Win10 20348+ APIs)
4. Media file decode (later phase; not required for first probe)
5. VST3 insert on a source chain

## Realtime rules

Audio callbacks must not:

- allocate heap
- touch filesystem/network/UI
- take blocking locks
- log with I/O
- sleep

Preallocate buffers. Prefer lock-free SPSC rings for UI↔engine sample transport. Control messages use a bounded non-blocking queue.

## Clocking

Do not assume shared clocks across devices. Plan for adaptive resampling / drift correction on long sessions.

## Protection

Transparent safety limiter on the broadcast bus before virtual output.
