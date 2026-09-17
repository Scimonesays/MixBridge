# Performance notes

Measured on Windows 11 + RME Fireface UC (2026-09-17):

| Metric | Value |
|--------|-------|
| Engine format | 48 kHz float32 stereo |
| Device rate (monitor) | 44.1 kHz shared |
| Buffer frames | 970 |
| Estimated latency | ~22 ms |
| Harness xruns (2.5 s) | 0 |
| Soak 2 min xruns | 0 |
| Soak 2 min WS | ~12.4 MB stable (+0.22 MB) |
| IPC meter poll | non-realtime; does not join audio callback |

Targets unchanged: near-zero UI impact on audio; no unbounded memory growth; measure latency — never guess.
