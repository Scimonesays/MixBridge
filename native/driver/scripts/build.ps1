#Requires -Version 5.1
<#
.SYNOPSIS
  Build the MixBridge kernel virtual audio driver (PortCls + KMDF miniport).

.DESCRIPTION
  Requires Visual Studio 2022 with the Windows Driver Kit (WDK) and the
  WindowsKernelModeDriver10.0 platform toolset. Does not modify global
  security settings (test-signing is documented separately in README.md).

.PARAMETER Configuration
  Debug or Release (default: Release).

.PARAMETER Platform
  x64 or ARM64 (default: x64).

.EXAMPLE
  pwsh native/driver/scripts/build.ps1
  pwsh native/driver/scripts/build.ps1 -Configuration Debug -Platform x64
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$DriverRoot = Split-Path $PSScriptRoot -Parent
$Solution = Join-Path $DriverRoot 'MixBridgeDriver.sln'

function Find-MsBuild {
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    throw 'MSBuild not found. Install Visual Studio 2022 Build Tools or add MSBuild to PATH.'
}

function Test-WdkPrerequisites {
    $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
    $kmInc = Join-Path $sdkRoot 'Include\10.0.26100.0\km\portcls.h'
    $ntddk = Join-Path $sdkRoot 'Include\10.0.26100.0\km\ntddk.h'

    Write-Host 'WDK / SDK header probe:'
    Write-Host "  ntddk.h   : $(Test-Path $ntddk)"
    Write-Host "  portcls.h : $(Test-Path $kmInc)"

    $wdfHeader = Get-ChildItem (Join-Path $sdkRoot 'Include') -Recurse -Filter 'wdf.h' -ErrorAction SilentlyContinue | Select-Object -First 1
    Write-Host "  wdf.h     : $(if ($wdfHeader) { $wdfHeader.FullName } else { 'NOT FOUND (KMDF headers — install full WDK VS extension)' })"
}

$msbuild = Find-MsBuild
Write-Host "Using MSBuild: $msbuild"
Test-WdkPrerequisites

$props = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/v:minimal"
)

if ($Platform -eq 'ARM64') {
    $props += @(
        '/p:RunCodeAnalysis=false',
        '/p:DriverTargetPlatform=Universal',
        '/p:UseInfVerifierEx=false',
        '/p:ValidateDrivers=false',
        '/p:StampInf=false',
        '/p:ApiValidator_Enable=false',
        '/p:InfVerif_Enable=false',
        '/p:DisableVerification=true',
        '/p:SignMode=Off',
        '/p:EnableInf2cat=false'
    )
}

Write-Host "Building $Solution ($Configuration | $Platform) ..."
& $msbuild $Solution @props /t:Build
if ($LASTEXITCODE -ne 0) {
    Write-Error @"
Build failed (exit $LASTEXITCODE).

Common blockers on dev machines:
  - WindowsKernelModeDriver10.0 toolset missing → install WDK + VS extension
  - wdf.h missing → install WDK KMDF headers (check Include\wdf\kmdf\)
  - Driver signing → use test-signing path documented in native/driver/README.md
"@
}

$outDir = Join-Path $DriverRoot "$Platform\$Configuration\package"
Write-Host "Build succeeded. Package output: $outDir"
Get-ChildItem $outDir -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime
