param(
  [ValidateSet("Debug","Release")]
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Sdk = Join-Path $Root "third_party\vst3sdk"
$EngineBuild = Join-Path $Root "native\audio-engine\build"
$Desktop = Join-Path $Root "apps\desktop"

Push-Location $Root
try {
  if (-not (Test-Path $Sdk)) {
    & (Join-Path $PSScriptRoot "fetch-vst3sdk.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }

  cmake -S native/audio-engine -B $EngineBuild
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

  cmake --build $EngineBuild --config $Configuration --target mb-engine-ipc
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

  if ($Configuration -ne "Release") {
    throw "The installer bundles the Release engine. Use -Configuration Release for packaging."
  }

  Push-Location $Desktop
  try {
    npm ci
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    npm run tauri:build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
  finally {
    Pop-Location
  }
}
finally {
  Pop-Location
}

Write-Host ""
Write-Host "MixBridge Windows bundle complete."
Write-Host "Installer output: apps\desktop\src-tauri\target\release\bundle\nsis"
