param(
  [ValidateSet("Debug","Release")]
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Sdk = Join-Path $Root "third_party\vst3sdk"
$EngineBuild = Join-Path $Root "native\audio-engine\build"
$Desktop = Join-Path $Root "apps\desktop"

function Initialize-MsvcEnvironment {
  $hasCppIncludes = $env:INCLUDE -and ($env:INCLUDE -match "\\VC\\Tools\\MSVC\\")
  if ($env:VSCMD_VER -and $hasCppIncludes) {
    return
  }

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Installer/vswhere was not found. Install Visual Studio 2022 Build Tools with 'Desktop development with C++'."
  }

  $vs = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
  if (-not $vs) {
    throw "MSVC C++ build tools were not found. In Visual Studio Installer, install 'Desktop development with C++' (MSVC v143 + Windows SDK)."
  }

  $devShell = Join-Path $vs "Common7\Tools\Launch-VsDevShell.ps1"
  if (-not (Test-Path $devShell)) {
    throw "Visual Studio developer shell was not found at: $devShell"
  }

  Write-Host "Initializing Visual Studio x64 C++ build environment..."
  & $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

  $hasCppIncludes = $env:INCLUDE -and ($env:INCLUDE -match "\\VC\\Tools\\MSVC\\")
  if (-not $hasCppIncludes) {
    throw "Visual Studio C++ headers are not available in this shell. Open Visual Studio Installer and add 'Desktop development with C++', including MSVC v143 and a Windows 10/11 SDK."
  }

  $cstdint = $env:INCLUDE.Split(';') |
    Where-Object { $_ } |
    ForEach-Object { Join-Path $_ "cstdint" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
  if (-not $cstdint) {
    throw "MSVC is present, but the C++ standard library headers are missing (cstdint not found). Repair/install the MSVC v143 C++ toolset in Visual Studio Installer."
  }
}

Push-Location $Root
try {
  Initialize-MsvcEnvironment

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

  # Tauri bundles the engine from build\Release. Multi-config generators (Visual Studio)
  # naturally place it there, while single-config generators such as Ninja place the
  # executable directly in the build root. Normalize both layouts so local builds and
  # CI package the exact same resource path.
  $ExpectedEngine = Join-Path $EngineBuild "Release\mb-engine-ipc.exe"
  if (-not (Test-Path $ExpectedEngine)) {
    $SingleConfigEngine = Join-Path $EngineBuild "mb-engine-ipc.exe"
    if (-not (Test-Path $SingleConfigEngine)) {
      throw "mb-engine-ipc.exe was built but could not be found in '$EngineBuild' or '$EngineBuild\Release'."
    }

    $ExpectedEngineDir = Split-Path -Parent $ExpectedEngine
    New-Item -ItemType Directory -Force -Path $ExpectedEngineDir | Out-Null
    Copy-Item -Force $SingleConfigEngine $ExpectedEngine
    Write-Host "Staged single-config engine for Tauri bundle: $ExpectedEngine"
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
