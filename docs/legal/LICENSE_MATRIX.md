# License matrix

MixBridge application SPDX: **MIT** (`LICENSE`).

This matrix is the license firewall for inbound dependencies and research sources.  
Exact upstream commits: `research/findings/upstream-commits.json`.

## Production policy

Allowed into production components when notices are satisfied:

- MIT
- Apache-2.0
- BSD-2-Clause / BSD-3-Clause
- ISC
- MS-PL **only** in isolated components (`native/driver/` or clearly marked MS-PL trees) with NOTICE compliance

Forbidden as source imports into MIT application/engine code without a separate legal plan:

- GPL / LGPL / AGPL
- Commons Clause / source-available / proprietary SDKs without explicit review
- Bundled third-party installers (e.g. VB-CABLE) without redistribution compliance

## Upstream matrix

| Project | Verified SPDX / terms | Commit | May enter MixBridge code? | Component if used | Notices |
|---------|------------------------|--------|---------------------------|-------------------|---------|
| MixBridge | MIT | — | Yes | all first-party | `LICENSE` |
| MicDeck / Virtual-Soundboard | MIT | `4ec7e53` | Patterns only; no wholesale import | engine design notes | attribute if code adapted |
| wasamix | MIT OR Apache-2.0 | `80b4b828` | Yes if adapted | native/probes, engine ideas | yes |
| Virtual-Audio-Driver | MIT | `bb34fba` | Yes if adapted | `native/driver/` | yes |
| Windows-classic-samples | MIT | `434f600` | Yes | probes / engine loopback | Microsoft MIT notice |
| Windows-driver-samples (SysVAD) | MS-PL | `97429c5` | Isolated only | `native/driver/` | MS-PL NOTICE |
| cpal | Apache-2.0 | `c811531` | Yes as crate dep if chosen | tools/crates | Apache NOTICE |
| vst3sdk | MIT (re-verify on upgrade) | `3cdf9ca` | Yes | `native/vst3-host/` | Steinberg MIT + trademark guidelines |
| rust-vst3-host | MIT | `49fc189` | Yes if chosen | SAFE host path | yes |
| truce-rack | MIT OR Apache-2.0 | `233f56c` | Ideas only for now | — | if adapted |
| Jamulus | AGPL-3.0-or-later (+ older GPL) | `cc40a8a` | **No code** | — | reference only |
| SonoBus | GPL-3.0 | `35f1062` | **No code** | — | reference only |
| VB-CABLE | Proprietary | n/a | External optional backend only | runtime detection | do not bundle until reviewed |
| ASIO SDK | Proprietary / Steinberg terms | n/a | Feature-gated, isolated, after review | future backend | quarantine SDK files |

## Rules of engagement

1. Prefer linking/adapting MIT/Apache sources with attribution in `third_party/THIRD_PARTY_NOTICES.md`.
2. Never paste Jamulus/SonoBus source into MixBridge trees.
3. Keep MS-PL SysVAD-derived code out of shared MIT libraries unless dual-licensed intentionally and documented.
4. Pin VST3 SDK commit; re-read `LICENSE.txt` before upgrades.
5. Every copied or substantially adapted module must record provenance (path + upstream commit) in THIRD_PARTY_NOTICES or file header SPDX.
