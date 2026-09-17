# Product doctrine (locked)

These are product decisions, not suggestions.

## Primary user goal

A returning user with a saved setup should go from desktop to ready for friends on Discord in **under 60 seconds**, ideally **under 15 seconds** once apps are open.

## Language

User-facing:

- **Standby** / **Go Live** / **On Air** / **Off Air / Ready**

Never: Perform / Performance Mode / Start Performance.

## Visual flow

**Sources → Mix → Outputs** (left to right). Not a DAW console. Not Voicemeeter.

Identity: modern neon broadcast studio + restrained retro radio control room.

- near-black / graphite
- electric cyan / blue accents
- restrained violet / magenta secondary
- strong ON AIR contrast
- MixBridge logo as core identity

## Hard UX rules

1. **Icon-first** — no text buttons when a universal icon communicates the action.
2. **No helper paragraphs** on the main surface — if needed, redesign the workflow.
3. **Visual state is communication** — glow, meters, dimming, pulse.
4. Hover tooltips + accessibility names + keyboard focus are required.

## Routing

Monitor and Live/Broadcast are independent first-class routes.

## Engine vs Live state (locked)

These are **not** the same:

```text
engine:    offline | starting | running | recovering | failed
broadcast: standby | live
```

- **Standby does not stop the engine.** Monitoring, meters, FX, and level work continue.
- **Go Live** enables the broadcast path only. It is never an alias for engine start.
- **Standby** (from On Air) disables broadcast only. It is never an alias for engine stop.
- Do not show **On Air** unless a real live destination is ready and broadcast is enabled.
- Engine start/stop belong in Advanced / Diagnostics, not the primary surface.

## Icon language (locked)

Obvious universal symbols replace unnecessary words: `+` add, trash remove, gear advanced, mute, headphones monitor, broadcast/live route.

## Future (documented, not Phase 4)

- **MixBridge Send VST3** for DAW insert sends
- Built-in fun starter instruments (original, MIT-clean; Vital/Helm are research only)
- Saved presets such as Discord Jam remembering full session state
- Guitar → interface → MixBridge → Guitar Rig VST3 → Live Mix

See `docs/ROADMAP.md` Phase 4+.
