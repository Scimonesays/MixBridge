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

## Future (documented, not Phase 3)

- **MixBridge Send VST3** for DAW insert sends
- Built-in fun starter instruments (original, MIT-clean; Vital/Helm are research only)
- Saved presets such as Discord Jam remembering full session state
- Guitar → interface → MixBridge → Guitar Rig VST3 → Live Mix

See `docs/ROADMAP.md` Phase 4+.
