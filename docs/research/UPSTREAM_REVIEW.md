# Upstream review

Date: 2026-09-17  
Exact clone hashes: `research/findings/upstream-commits.json`  
Trees: `research/upstream/` (gitignored)

License verification used repository LICENSE/COPYING files at the cloned commit, not GitHub’s SPDX badge alone.

---

## 3godzinyL/Virtual-Soundboard-Audio-Windows-Mixer (MicDeck)

| Field | Value |
|-------|-------|
| Purpose | Windows soundboard + virtual mic mixer; Tauri + Rust + C++20 WASAPI |
| License | MIT (LICENSE, © 2026 MicDeck contributors) |
| Latest activity | 2026-07-31 (commit `4ec7e53`) |
| Relevant subsystem | Dedicated native engine process, process loopback, VB-CABLE/VAD routing, shared-memory IPC, Tauri UI |
| Code worth studying | `native-audio/engine` WASAPI capture/render; process-loopback start path; realtime callback rules; engine heartbeat |
| Legal reuse | MIT allows reuse with attribution |
| Attribution required | Yes |
| Risks | Closest existing product shape — temptation to copy wholesale; some UI/docs in Polish; depends on external virtual cable for virtual mic |
| Decision | **ADAPT** (architecture + WASAPI patterns only). Do not import the product tree as MixBridge. Reimplement with MixBridge naming, UX, and license notices. |

---

## ytchenak/wasamix

| Field | Value |
|-------|-------|
| Purpose | Tray mixer: mic + system loopback → VB-Audio Virtual Cable |
| License | Dual MIT / Apache-2.0 (`LICENSE-MIT`, `LICENSE-APACHE`) |
| Latest activity | Commit `80b4b828` (2026-04-26); repo still active mid-2026 |
| Relevant subsystem | Rust WASAPI capture threads, ring buffers, simple mixer, device enumeration |
| Code worth studying | Loopback init fallbacks across render devices; pipeline thread model; VB-Cable discovery |
| Legal reuse | Permissive dual license |
| Attribution required | Yes if code adapted |
| Risks | Hard dependency on proprietary VB-CABLE; no process loopback; no VST3; tray UX ≠ MixBridge product |
| Decision | **REFERENCE / ADAPT** patterns for WASAPI loopback reliability. Do not adopt VB-CABLE as the only virtual-output path. |

---

## VirtualDrivers/Virtual-Audio-Driver

| Field | Value |
|-------|-------|
| Purpose | WDK virtual speaker + virtual microphone |
| License | MIT (© 2025 MikeTheTech) |
| Latest activity | Commit `bb34fba` (2026-03-03) |
| Relevant subsystem | Kernel virtual endpoints; install/test-signing notes |
| Code worth studying | Endpoint topology, format support matrix, install scripts |
| Legal reuse | MIT with attribution |
| Attribution required | Yes |
| Risks | Requires WDK + test signing / production EV signing; beta; commercial custom-build pitch in README; MixBridge machine currently lacks WDK km headers |
| Decision | **ADAPT** as primary candidate for `native/driver/` once WDK is available. Until then: **fallback backends** (compatible virtual cable / test loopback). Driver signing is a packaging blocker, not a stop. |

---

## microsoft/Windows-classic-samples

| Field | Value |
|-------|-------|
| Purpose | Official Win32 API samples |
| License | MIT (Microsoft) |
| Latest activity | Commit `434f600` (2026-09-03) |
| Relevant subsystem | `Samples/ApplicationLoopback` — process-tree loopback via `ActivateAudioInterfaceAsync` (Win10 20348+) |
| Code worth studying | `LoopbackCapture.cpp/h`, process include/exclude tree flags |
| Legal reuse | MIT |
| Attribution required | Yes when adapting |
| Risks | Sample quality ≠ product hardening; API availability floor is build 20348 |
| Decision | **USE / ADAPT** as the authoritative process-loopback reference for MixBridge probes and engine. |

---

## microsoft/Windows-driver-samples

| Field | Value |
|-------|-------|
| Purpose | Official WDK driver samples |
| License | MS-PL |
| Latest activity | Commit `97429c5` (2026-09-16) |
| Relevant subsystem | `audio/sysvad` virtual audio device sample |
| Code worth studying | SysVAD topology, miniport patterns, format negotiation |
| Legal reuse | MS-PL — keep driver code isolated under `native/driver/` with notices |
| Attribution required | Yes; satisfy MS-PL for that component |
| Risks | MS-PL isolation discipline; WDK required |
| Decision | **REFERENCE / ADAPT** for MixBridge VAD design. Isolate MS-PL material; do not mix into MIT-only crates without notices. |

---

## RustAudio/cpal

| Field | Value |
|-------|-------|
| Purpose | Cross-platform audio I/O in Rust |
| License | Apache-2.0 (`LICENSE`) |
| Latest activity | Commit `c811531` (2026-09-11) |
| Relevant subsystem | Device enumeration abstractions; WASAPI backend |
| Code worth studying | Host/device model; stream config negotiation |
| Legal reuse | Apache-2.0 |
| Attribution required | Yes if depended upon or adapted |
| Risks | Cross-platform generality can obscure Windows-specific low-latency controls MixBridge needs |
| Decision | **REFERENCE**. Prefer dedicated C++ WASAPI engine for realtime path; may use cpal later for non-realtime probes or Rust tooling only if it does not regress latency control. |

---

## steinbergmedia/vst3sdk

| Field | Value |
|-------|-------|
| Purpose | Official VST 3 SDK |
| License | MIT (`LICENSE.txt`, © Steinberg Media Technologies GmbH) at commit `3cdf9ca` (SDK 3.8.x) |
| Latest activity | 2026-08-11 |
| Relevant subsystem | Hosting interfaces, plug-in scanning, editor hosting |
| Code worth studying | Hosting examples; moduleinfo; processor/controller split |
| Legal reuse | MIT at this commit — **re-verify before every SDK upgrade**; respect `VST3_Usage_Guidelines.pdf` trademarks |
| Attribution required | Yes; follow Steinberg trademark/usage guidelines |
| Risks | Submodules required for full build; trademark restrictions on “VST”; SDK license historically changed — pin + re-check |
| Decision | **USE** (pinned, re-verified) as the VST3 hosting foundation. Do not redistribute commercial plugins. |

---

## HelgeSverre/rust-vst3-host

| Field | Value |
|-------|-------|
| Purpose | Safe Rust VST3 host library with crash isolation goals |
| License | MIT |
| Latest activity | Commit `49fc189` (2026-08-09) |
| Relevant subsystem | Discover/load/process; parameter automation; crash isolation |
| Code worth studying | Host API shape; isolation strategy |
| Legal reuse | MIT |
| Attribution required | Yes if used |
| Risks | Maturity vs Steinberg C++ hosting; latency of isolation modes |
| Decision | **REFERENCE / EVALUATE** for SAFE-mode isolation. LIVE-mode hosting may remain native C++ beside the Steinberg SDK. |

---

## truce-audio/truce-rack

| Field | Value |
|-------|-------|
| Purpose | Audio rack / plugin hosting related project |
| License | Dual MIT / Apache-2.0 |
| Latest activity | Commit `233f56c` (2026-06-19) |
| Relevant subsystem | Rack/graph ideas |
| Code worth studying | Modular rack organization |
| Legal reuse | Permissive |
| Attribution required | Yes if adapted |
| Risks | Smaller/younger project; may not map to MixBridge UX |
| Decision | **REFERENCE ONLY** for graph/rack structure ideas. |

---

## jamulussoftware/jamulus

| Field | Value |
|-------|-------|
| Purpose | Real-time networked ensemble jamming |
| License | **AGPL-3.0-or-later** (new code); older code GPL-3.0-or-later (`COPYING`) |
| Latest activity | Commit `cc40a8a` (2026-09-14) |
| Relevant subsystem | Low-latency network audio, Opus, jitter buffers |
| Code worth studying | Concepts only: buffering, latency UI honesty, session recovery |
| Legal reuse | **Do not copy code** into MIT MixBridge components |
| Attribution required | N/A for reference-only study |
| Risks | Copyleft contamination |
| Decision | **REFERENCE ONLY** (future Jam mode ideas). |

---

## sonosaurus/sonobus

| Field | Value |
|-------|-------|
| Purpose | Peer-to-peer network audio collaboration |
| License | **GPL-3.0** + app-store exception |
| Latest activity | Commit `35f1062` (2026-09-04) |
| Relevant subsystem | P2P audio, Opus, mix controls, recording |
| Code worth studying | UX for remote mix; network quality displays |
| Legal reuse | **Do not copy code** |
| Attribution required | N/A for reference-only |
| Risks | GPL contamination |
| Decision | **REFERENCE ONLY**. |

---

## Additional notes (not cloned)

| Candidate | Note | Decision |
|-----------|------|----------|
| VB-CABLE | Proprietary/donationware virtual cable | Optional **external fallback backend** only; do not bundle without redistribution review |
| ASIO SDK | Steinberg SDK with separate licensing constraints | Isolate behind feature boundary until license review complete |
| PlugAndVoice / Commons Clause projects | Source-available restrictions | **REJECT** for code reuse |

---

## Summary decisions

| Project | Decision |
|---------|----------|
| MicDeck / Virtual-Soundboard | ADAPT patterns |
| wasamix | ADAPT loopback reliability ideas |
| Virtual-Audio-Driver | ADAPT when WDK ready; fallback until then |
| Windows ApplicationLoopback sample | USE/ADAPT |
| Windows SysVAD sample | REFERENCE/ADAPT (MS-PL isolate) |
| cpal | REFERENCE |
| VST3 SDK | USE (pin + re-verify) |
| rust-vst3-host | EVALUATE for SAFE mode |
| truce-rack | REFERENCE |
| Jamulus | REFERENCE ONLY |
| SonoBus | REFERENCE ONLY |
