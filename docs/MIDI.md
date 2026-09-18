# MIDI input (Phase 7+)

## Goal

Basic MIDI for starter instruments and VST3 instruments:

- note on / note off / velocity
- sustain where reasonable
- device selection + hotplug

## Status

Not implemented yet. Engine currently hosts audio FX; MIDI event injection into VST3 `IEventList` is the next hosting step after virtual output unblocks.

## Non-goals

Do not turn MixBridge into a full MIDI DAW.
