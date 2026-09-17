# Troubleshooting

## WDK / virtual driver

**Symptom:** Cannot build `native/driver/`.  
**Cause:** Windows Driver Kit km headers not installed on the current machine.  
**Action:** Install WDK matching installed Windows SDK. Until then, use test loopback / optional external virtual cable backends. Do not stop app development.

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
