# Performance notes

Targets (optimize after correctness):

- Near-zero UI impact on audio
- No accumulating memory growth
- No periodic audio stalls
- Modest UI footprint (Tauri preferred over Electron)
- Measure real latency; never guess

Buffer size options: 64 / 128 / 256 / 512 (device-capable only).

X-run counters must be exposed in diagnostics.
