# MixBridge QA — Phase 4.1 progress

Date: 2026-09-17T23:25:00Z

## Phase 3

Gates 3.1–3.12 remain **PASS** (not reopened).

## Phase 4.1 checklist

| # | Check | Status | Evidence |
|---|-------|--------|----------|
| 1 | Engine remains running in Standby | **PASS** | `prove-phase41-ipc.ps1`; STATUS STATE running + BROADCAST standby |
| 2 | Standby monitoring path stays active | **PASS** | START then BROADCAST_ENABLE fails; engine stays running |
| 3 | Go Live ≠ engine lifecycle | **PASS** | BROADCAST_ENABLE / DISABLE; START/STOP independent |
| 4 | No On Air without live destination | **PASS** | LIVE_DEST 0; BROADCAST_ENABLE → `no_live_destination` |
| 5 | `+` opens source picker | **PASS** | UI: Input / Application choices (desktop shell) |
| 6 | Physical input via picker/IPC | **PASS** | ADD_PHYSICAL → OK ID; LIST_CAPTURE ID+NAME |
| 7 | Application list via IPC | **PASS** | LIST_PROCESSES implemented (visible-window apps) |
| 8 | Two source cards coexist | **PASS** | multi-source Map UI + LIST_SOURCES |
| 9–11 | Independent meter/gain/mute | **PASS** | per-id METER_SOURCE / SET_GAIN / SET_MUTE; remove preserves peer mute |
| 12 | Trash removes only selected | **PASS** | REMOVE + phase41_state_test |
| 13 | Tone not user-facing `+` | **PASS** | `+` → picker only; ADD_TONE diagnostics/IPC only |
| 14–15 | Offline UI fonts | **PASS** | Google Fonts removed; system Segoe UI Variable |
| 16 | A11y meter not aria-hidden | **PASS** | master meter outside aria-hidden ancestor |
| 17 | No helper prose | **PASS** | identity/state only |

## Commands

```text
ctest --test-dir native/audio-engine/build --output-on-failure
powershell -File scripts/prove-phase41-ipc.ps1
```

## Remaining Phase 4+ gaps

- Real Live destination (Phase 6 virtual output) before On Air can activate
- Application picker icons (names only today)
- Session/preset persistence
- VST3 hosting (Phase 5)
- Packaging / final acceptance matrix
