# MixBridge test plan & acceptance gates

Status legend: PASS / FAIL / BLOCKED / NOT RUN

## Automated (CI-capable)

| Test | Command | Status |
|------|---------|--------|
| DSP silence / tone / impulse | `scripts/run-audio-tests.ps1` (ctest) | NOT RUN until first green build |
| IPC / config | TBD Phase 3+ | NOT RUN |
| VST3 fixture | TBD Phase 5 | NOT RUN |

## Hardware / local acceptance gates

| Gate | Description | Status |
|------|-------------|--------|
| 1 Clean Build | Fresh checkout builds per `docs/SETUP.md` | PARTIAL — native probes build + DSP/ctest PASS (2026-09-17) |
| 2 Clean Launch | App launches without hacks | NOT RUN (no UI yet) |
| 3 Physical Audio | Capture/monitor/meter/mute/route | NOT RUN (probe capture only) |
| 4 Application Capture | Deterministic test player capture | PARTIAL — process loopback probe PASS with `mb-tone-player` |
| 5 VST3 | Fixture load/process/bypass/state | NOT RUN |
| 6 Virtual Output | Endpoint + analysis | NOT RUN (WDK missing; fallback planned) |
| 7 Real Performance | Guitar + FX + backing | NOT RUN |
| 8 Stability | 30+ min soak | NOT RUN |
| 9 UX | Fresh-user flow without docs | NOT RUN |
| 10 Repository Quality | Docs current | PARTIAL — core docs present; expand as phases complete |

**10/10 requires all ten PASS with evidence under `artifacts/qa/`.**

## Evidence layout

```text
artifacts/qa/latest/
  SUMMARY.md
  environment.json
  devices.json
  test-results.json
  ...
```
