# Third-party notices

MixBridge first-party code is licensed under the MIT License. See root `LICENSE`.

This file records third-party software that MixBridge may depend on, adapt, or reference. Research clones under `research/upstream/` are **not** redistributed with MixBridge releases.

## Notices for adapted or planned dependencies

### Microsoft Windows classic samples (ApplicationLoopback)

- Project: https://github.com/microsoft/Windows-classic-samples
- Commit reviewed: `434f6002bdf9cf9829406c3ff2b33387982d6168`
- License: MIT
- Use: Process/application loopback capture reference for probes and engine

### Microsoft Windows driver samples (SysVAD)

- Project: https://github.com/microsoft/Windows-driver-samples
- Commit reviewed: `97429c5623590d52f001249460daf43e6749d777`
- License: Microsoft Public License (MS-PL)
- Use: Architectural reference for virtual audio device work under `native/driver/`
- Note: Any MS-PL code incorporated must remain license-compliant and isolated

### Steinberg VST3 SDK

- Project: https://github.com/steinbergmedia/vst3sdk
- Commit reviewed: `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`
- License: MIT (verify again on upgrade)
- Trademark: Follow Steinberg VST usage guidelines; do not misuse VST trademarks
- Use: Official hosting SDK for MixBridge VST3 support

### Virtual Audio Driver (MikeTheTech)

- Project: https://github.com/VirtualDrivers/Virtual-Audio-Driver
- Commit reviewed: `bb34fba15faf569a6ae9bdea360bc1cf4821354e`
- License: MIT
- Use: Candidate patterns for MixBridge virtual endpoints (subject to WDK/signing)

### MicDeck / Virtual-Soundboard-Audio-Windows-Mixer

- Project: https://github.com/3godzinyL/Virtual-Soundboard-Audio-Windows-Mixer
- Commit reviewed: `4ec7e53c0ac37291187d92f3d75bf88af469895c`
- License: MIT
- Use: Architectural reference (dedicated WASAPI engine process, process loopback). MixBridge does not vendor this tree.

### wasamix

- Project: https://github.com/ytchenak/wasamix
- Commit reviewed: `80b4b828d5d083c6670297f11564bd69e7230f24`
- License: MIT OR Apache-2.0
- Use: WASAPI loopback reliability patterns

### cpal

- Project: https://github.com/RustAudio/cpal
- Commit reviewed: `c81153143724d91fce4161a15cb895b2cfab7da7`
- License: Apache-2.0
- Use: Optional reference / possible non-realtime tooling dependency

### rust-vst3-host

- Project: https://github.com/HelgeSverre/rust-vst3-host
- Commit reviewed: `49fc189177c72cf799d5034fcca81d8aacc3d7a6`
- License: MIT
- Use: Evaluation candidate for SAFE-mode plugin isolation

### truce-rack

- Project: https://github.com/truce-audio/truce-rack
- Commit reviewed: `233f56c7788291b2f24e3f2aba3a767330f72cb7`
- License: MIT OR Apache-2.0
- Use: Reference only

## Reference-only (no code incorporation)

### Jamulus

- License: AGPL-3.0-or-later (newer) / GPL-3.0-or-later (older)
- Commit reviewed: `cc40a8a9442879a39dd9fda3ee4f61fccde2c2ba`
- Status: Ideas only — networking/latency concepts for future Jam mode

### SonoBus

- License: GPL-3.0
- Commit reviewed: `35f1062dab196b9838a4bb529c4bf6592b7f5987`
- Status: Ideas only

## External proprietary backends (not bundled)

- VB-Audio Virtual Cable — optional user-installed fallback virtual output
- ASIO — deferred behind license review and isolated backend boundary

## Update rule

When adding a dependency or adapting upstream source, append a dated entry with project URL, exact commit/version, SPDX, and MixBridge path.
