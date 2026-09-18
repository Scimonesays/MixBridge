# MixBridge developer setup

Generated from Phase 0 environment detection on the development machine.

## Required toolchain

| Tool | Detected / target | Notes |
|------|-------------------|--------|
| Windows | 11 x64 (10.0.26200+) | Primary product platform |
| Git | 2.47+ | Required |
| GitHub CLI | optional | Useful for research |
| Visual Studio 2022 Build Tools | 17.14+ with MSVC x64 | `cl` 19.44 confirmed |
| Windows SDK | 10.0.26100 (also 22621/22000/20348) | WASAPI / classic desktop |
| CMake | 4.3+ | Native builds |
| Node.js | 24.x LTS line | Tauri frontend |
| npm | 11.x | Frontend packages |
| Rust + Cargo | **install required** | Tauri 2 + crates |
| WDK km headers | **partial** (SDK 10.0.26100 `ntddk.h`, `portcls.h`) | Driver scaffold in `native/driver/` |
| WDK VS toolset | **not detected** (`WindowsKernelModeDriver10.0`) | Required to compile driver; use WASAPI fallback until installed |

## Install Rust (required)

```powershell
winget install Rustlang.Rustup
# restart shell, then:
rustup default stable-x86_64-pc-windows-msvc
rustc --version
cargo --version
```

## Optional but recommended

```powershell
winget install Ninja-build.Ninja
```

## Verify MSVC environment

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
cl
cmake --version
```

## WDK / driver signing

First-party virtual endpoints (**MixBridge Output** / **MixBridge Input**) live in `native/driver/`.

Build requirements:

1. Windows Driver Kit + Visual Studio extension (`WindowsKernelModeDriver10.0` toolset)
2. KMDF headers (`wdf.h` under `Include\wdf\kmdf\`) — present on this machine
3. Test signing for local install (`native/driver/README.md`); production requires EV / Hardware Dev Center signing

Until the driver is installed, Live broadcast uses **WASAPI render to a user-selected endpoint** (Phase 6.1). See `docs/AUDIO_PIPELINE.md`.

If WDK compile or production signing is unavailable:

1. Document the blocker in `docs/TROUBLESHOOTING.md` and QA evidence.
2. Continue with validated user-mode fallback backends (compatible virtual cable / test loopback).
3. Do not stop application development.

## Research clones

Upstream reference trees live under `research/upstream/` and are **gitignored**. Clone with:

```powershell
pwsh scripts/clone-upstream.ps1
```

## Assumptions (reversible)

1. MixBridge application SPDX: `MIT`.
2. Preferred UI shell: Tauri 2 (not Electron) unless measured evidence forces reconsideration.
3. Preferred realtime engine language: C++20 for WASAPI proximity and deterministic latency control.
4. Driver signing is a packaging blocker, not a product-development stopper.
