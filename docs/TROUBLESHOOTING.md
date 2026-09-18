# Troubleshooting

## WDK / virtual driver

**Symptom:** `MSB8020: WindowsKernelModeDriver10.0 build tools cannot be found` when running `native/driver/scripts/build.ps1`.  
**Cause:** WDK Visual Studio extension not installed (SDK km headers alone are insufficient).  
**Action:** Install [WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) matching SDK 10.0.26100. Re-run build script.

**Symptom:** Compile errors on `#include <wdf.h>`.  
**Cause:** KMDF include path not on machine (check `Windows Kits\10\Include\wdf\kmdf\`).  
**Action:** Install full WDK VS extension. Architecture is PortCls + KMDF miniport (MIT Virtual-Audio-Driver pattern).

**Symptom:** Driver build succeeds but Windows refuses to load `.sys`.  
**Cause:** Unsigned kernel driver (production EV signing not configured).  
**Action:** Use test-signing path in `native/driver/README.md` (`bcdedit /set testsigning on`, reboot, `install-testsign.ps1`). Do not disable unrelated security policies.

**Symptom:** Discord has no MixBridge mic.  
**Cause:** Driver not installed; Live still on WASAPI render / external cable path.  
**Action:** Select a working render endpoint in MixBridge Live settings, or install test-signed driver and choose **MixBridge Output** as mic input.

## Process loopback fails

**Symptom:** `process-loopback` returns an error HRESULT.  
**Checks:**
- Windows 10 build 20348+ / Windows 11
- Target process is actively rendering audio
- Run probe elevated only if policy requires (normally not needed)

## System loopback near-silence

Default render device may be muted, set to a disabled Bluetooth endpoint, or exclusive-mode owned. Try headphones/interface explicitly in later device-select UI.

## MSVC not found in shell

Use `Launch-VsDevShell.ps1 -Arch amd64` or `scripts/build-probes.ps1`.

## Rust/cargo not found

Install Rustup (`docs/SETUP.md`), restart the shell, `rustup default stable-x86_64-pc-windows-msvc`.
