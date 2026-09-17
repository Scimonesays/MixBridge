# Build MixBridge native probes (MSVC + CMake + Ninja).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "native\probes\build"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "MSVC Build Tools not found" }

$devShell = Join-Path $vs "Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path $devShell)) {
  # Fallback: VsDevCmd
  $vsDevCmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
  cmd /c "`"$vsDevCmd`" -arch=amd64 && cmake -S `"$Root\native\probes`" -B `"$Build`" -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build `"$Build`""
  exit $LASTEXITCODE
}

& $devShell -Arch amd64 -SkipAutomaticLocation
Set-Location $Root
New-Item -ItemType Directory -Force -Path $Build | Out-Null
cmake -S "$Root\native\probes" -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $Build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Built probes in $Build"
Get-ChildItem $Build -Filter "mb-*.exe" | Select-Object Name, Length, LastWriteTime
