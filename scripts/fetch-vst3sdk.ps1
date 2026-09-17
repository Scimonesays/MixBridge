# Fetch Steinberg VST3 SDK (MIT) into third_party/vst3sdk — not committed.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root "third_party\vst3sdk"
$Tag = "v3.7.12_build_20"

New-Item -ItemType Directory -Force -Path (Join-Path $Root "third_party") | Out-Null
if (Test-Path (Join-Path $Dest ".git")) {
  Write-Host "vst3sdk already present at $Dest"
} else {
  git clone --depth 1 --branch $Tag https://github.com/steinbergmedia/vst3sdk.git $Dest
}
Push-Location $Dest
git submodule update --init --recursive --depth 1
Pop-Location
Write-Host "vst3sdk_fetch_result=PASS tag=$Tag"
