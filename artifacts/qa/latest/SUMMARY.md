# Probe selftest results (Phase 2)

Date: 2026-09-17

## Deterministic DSP (ctest)

| Test | Result |
|------|--------|
| dsp_silence | PASS |
| dsp_440 | PASS |
| dsp_1000 | PASS |
| dsp_impulse | PASS |

## WASAPI probe selftest

| Step | Result | Evidence |
|------|--------|----------|
| enumerate | PASS | 5 capture + 5 render endpoints (incl. RME Fireface UC) |
| render 440 Hz | PASS | `render_ok=1` |
| capture | PASS | frames captured @ 48 kHz |
| system loopback | PASS | concurrent 1000 Hz tone; `dominant_hz=1000.00` |
| process loopback | PASS | `mb-tone-player` PID capture; `dominant_hz=1000.00` rms≈0.14 |

## Honest gate status after this run

| Gate | Status |
|------|--------|
| 1 Clean Build (probes) | PASS for probes; full app not yet |
| 2 Clean Launch | NOT RUN |
| 3 Physical Audio (product) | NOT RUN (probe capture only) |
| 4 Application Capture | PARTIAL — probe proved process loopback; product path not built |
| 5–10 | NOT RUN |

Do not claim 10/10.
