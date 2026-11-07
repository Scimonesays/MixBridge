# MixBridge Discord path proof

Generated: 2026-11-06T20:01:53-05:00

## Verdict

`LIVE_DEST 1` + `BROADCAST live` confirmed. Live WASAPI is writing to MixBridge Speakers (render peak ~0.10).

## Endpoints

| Role | Windows name | Device ID |
|------|--------------|-----------|
| Live (render / MixBridge Input) | Speakers (MixBridge Virtual Audio) | `{0.0.0.00000000}.{a996b872-b5d3-4815-a16f-eefcfb180410}` |
| Discord mic (capture / MixBridge Output) | Microphone Array (MixBridge Virtual Audio) | `{0.0.1.00000000}.{2f818797-c1d9-489c-bf70-397357bb8c69}` |

## Engine STATUS

```
OK STATE running BROADCAST live LIVE_DEST 1 FRAMES 7238496 XRUNS 0 UNDERRUNS 0 OVERRUNS 0 LAT_MS 22.00
```

## Meters

- Broadcast: `OK PEAK 0.099668 RMS 0.070130 CLIP 0`
- Tone source: `OK PEAK 0.200000 RMS 0.140477 CLIP 0`
- WASAPI render peak (MixBridge Speakers): `0.10`
- Capture endpoint meter without an active Discord/client open: `0` (open Discord or a recorder on the Mic Array to hear the cable)

## Discord

1. Discord → Voice Settings → Input Device = **Microphone Array (MixBridge Virtual Audio)**
2. MixBridge Go Live with sources broadcast-enabled
3. Friend should hear the Live mix

## App / session

- `preferMixBridgeLive` matches MixBridge Input and Speakers (MixBridge Virtual Audio)
- Discord hint shown when Live is MixBridge
- Persisted `live_device_id` in `com.mixbridge.app/last-session.json`

## Driver

- Package: `native/driver/x64/Release/package` (test-signed)
- Service: MixBridgeDriver RUNNING
- Parent: MixBridge Virtual Audio OK (`ROOT\MEDIA\0000`)
- HVCI off, testsigning Yes

STATUS `LIVE_DEST 1`
