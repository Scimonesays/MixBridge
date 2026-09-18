# MIDI input (Phase 7+)

## Goal

Basic MIDI for starter instruments and VST3 instruments:

- note on / note off / velocity
- sustain where reasonable
- device selection + hotplug

## Status

**Not implemented.** Tone / built-in instrument sources exist via `ADD_TONE` / `engine_add_tone`, but there is no winmm (or other) MIDI input path and no note→frequency / note→VST3 `IEventList` injection yet.

Next steps when scheduled:

1. Enumerate MIDI-in devices + hotplug.
2. Map note-on/off to the active built-in tone/instrument source (minimal product win).
3. Forward events into hosted VST3 instruments via `IEventList`.

## Non-goals

Do not turn MixBridge into a full MIDI DAW.
