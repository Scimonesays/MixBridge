# MixBridge virtual audio driver (Phase 6)

## Goal

Expose first-party Windows audio endpoints:

| Endpoint | Role | Visible in |
|----------|------|------------|
| **MixBridge Output** | Virtual microphone (capture) | Discord, OBS, Zoom input device list |
| **MixBridge Input** | Virtual speaker (render) | MixBridge engine Live render target |

Signal flow once integrated:

```text
MixBridge engine → MixBridge Input (render)
                        ↓ (kernel loopback)
                   MixBridge Output (capture) → Discord / OBS
```

Until the driver is installed, the product uses **WASAPI render to a user-selected endpoint** (Phase 6.1 virtual cable path). See `docs/AUDIO_PIPELINE.md`.

## Status (2026-09-17)

| Item | Status |
|------|--------|
| PortCls + WaveRT driver sources under `native/driver/` | **Scaffolded** (adapted from MIT Virtual-Audio-Driver patterns) |
| `MixBridgeDriver.inf` with friendly names | **Done** |
| Build scripts (`scripts/build.ps1`) | **Done** |
| km headers (`ntddk.h`, `portcls.h`) on dev machine | **Present** (SDK 10.0.26100.0) |
| KMDF headers (`wdf.h`) | **Present** (`Include\wdf\kmdf\1.15\wdf.h`; not under `km\`) |
| `WindowsKernelModeDriver10.0` toolset | **Missing** — primary compile blocker |
| Production EV code signing | **Blocked** (expected; use test-signing for dev) |
| Engine wired to MixBridge Input automatically | **Not yet** — still user-selected WASAPI render |

## Architecture choice

Headers probed on this machine:

- `portcls.h` — **yes** → PortCls / WaveRT miniport (SysVAD-style)
- `wdf.h` — **yes** (`Include\wdf\kmdf\1.15\`) → KMDF miniport wrapper available

The driver follows the upstream MIT pattern: **KMDF driver frame + PortCls audio miniports**. Building requires the full WDK Visual Studio extension (toolset + KMDF headers), not SDK km headers alone.

## Build

Prerequisites:

1. Visual Studio 2022 Build Tools or VS 2022
2. Windows SDK 10.0.26100+
3. **Windows Driver Kit** with VS extension (`WindowsKernelModeDriver10.0` toolset)

```powershell
# From repo root
pwsh native/driver/scripts/build.ps1
# Debug build:
pwsh native/driver/scripts/build.ps1 -Configuration Debug -Platform x64
```

Output (after successful build):

```text
native/driver/x64/Release/package/
  mixbridgedriver.sys
  MixBridgeDriver.inf
  mixbridgedriver.cat
```

**Current machine:** build fails with `MSB8020: WindowsKernelModeDriver10.0 build tools cannot be found`.

Install WDK: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk

## Test signing (development only)

Production shipping requires **Microsoft Hardware Dev Center** attestation / EV code signing. That is a packaging blocker, not a reason to stop app development.

For local dev (admin PowerShell):

```powershell
# One-time: enable test signing (reboot required). Does not weaken unrelated security policies.
bcdedit /set testsigning on

# After build succeeds:
pwsh native/driver/scripts/install-testsign.ps1
```

Manual install alternative:

```powershell
pnputil /add-driver native/driver/x64/Release/package/MixBridgeDriver.inf /install
```

Verify in **Settings → System → Sound**:

- **Output:** MixBridge Input
- **Input:** MixBridge Output

## Legal isolation

- Product driver sources: **MIT** (MixBridge)
- Patterns adapted from MIT Virtual-Audio-Driver (see `THIRD_PARTY_NOTICES.md`)
- SysVAD / MS-PL sample code stays in research clones only — not vendored into the engine

See `docs/legal/LICENSE_MATRIX.md`.

## Solution layout

```text
native/driver/
  MixBridgeDriver.sln
  Source/
    Main/       adapter, WaveRT miniports, MixBridgeDriver.inx
    Filters/    topology / wave filter descriptors
    Utilities/  hw simulation, tone generator
    Inc/        shared headers
  Package/      driver package project
  scripts/
    build.ps1
    install-testsign.ps1
```
