# MixBridge roadmap

## V1 complete application track

- **Environment / research / licensing:** done for V1.
- **Audio probes:** capture, render, system loopback, process loopback and deterministic DSP — done.
- **Realtime engine:** C++20/WASAPI graph, monitor + broadcast buses, meters, IPC, recovery states and soak harness — done.
- **Product shell:** icon-first Sources → Mix → Outputs, real source/output pickers, Standby/On Air state and automatic session restore — done.
- **VST3:** discovery, load/process, per-source insert, bypass, dry fallback, live native editor window, realtime parameter bridge, component/controller state snapshots and session restore — done.
- **Live output:** user-selected real Windows render/virtual-cable endpoint — done.
- **Packaging:** portable sidecar + NSIS release script + CI installer gate — implemented.
- **Starter instruments:** first-party Neon Keys + Soft Pad, polyphonic keyboard UI, normal FX/monitor/live routing and session restore — done.
- **Release UI:** final broadcast-console visual pass + README product screenshot — done.

## Future enhancements

- Multi-plugin chains instead of the V1 single insert.
- MixBridge Send VST3 for DAW-to-MixBridge routing.
- Expanded MIDI controller support and additional first-party instruments.
- Network/jam features.

## Separate signed-driver track

A first-party Windows capture endpoint named **MixBridge Output** remains a separate deliverable. It requires WDK integration, an actual user-mode → kernel audio transport, production/attestation signing, installer lifecycle proof and real Discord/OBS capture validation.

The V1 application does not depend on falsely claiming that driver exists; it uses a real selected Windows Live render endpoint.
