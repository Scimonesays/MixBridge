# MixBridge QA — Phase 4.1 + 6.1

Date: 2026-09-17T23:40:00Z

## Phase 3

Gates 3.1–3.12 remain **PASS**.

## Phase 4.1

All checklist items PASS (`scripts/prove-phase41-ipc.ps1`, `engine_phase41_state_test`).

## Phase 6.1 — Live WASAPI destination

| Check | Status | Evidence |
|-------|--------|----------|
| No On Air without destination | PASS | LIVE_DEST 0 → `no_live_destination` |
| Select live render device | PASS | `SET_LIVE_DEVICE` + LIST_RENDER |
| Go Live enables broadcast sink | PASS | BROADCAST live; engine stays running |
| Standby keeps monitor | PASS | BROADCAST standby + STATE running |
| First-party MixBridge VAD | BLOCKED | WDK not installed — see `native/driver/README.md` |

Honest note: Live path today renders the broadcast bus to a **user-selected WASAPI render endpoint** (e.g. installed virtual cable). Discord sees that endpoint, not a branded MixBridge Output until the WDK driver ships.

## Commands

```text
ctest --test-dir native/audio-engine/build --output-on-failure
powershell -File scripts/prove-phase41-ipc.ps1
```
