#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Install a minimal WindowsKernelModeDriver10.0 MSBuild platform toolset that
  imports the already-installed WDK 10.0.26100 kits props/targets.
#>
[CmdletBinding()]
param(
    [string] $VsInstance = 'C:\Program Files\Microsoft Visual Studio\2022\Community',
    [string] $WdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10",
    [string] $WdkVersion = '10.0.26100.0'
)

$ErrorActionPreference = 'Stop'

$vc = Join-Path $VsInstance 'MSBuild\Microsoft\VC\v170'
if (-not (Test-Path $vc)) {
    $vc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Microsoft\VC\v170'
}
if (-not (Test-Path $vc)) {
    throw "VC MSBuild targets not found under $VsInstance or BuildTools."
}

if (-not $WdkRoot.EndsWith('\') -and -not $WdkRoot.EndsWith('/')) {
    $WdkRoot += '\'
}

$props = @"
<?xml version="1.0" encoding="utf-8"?>
<!-- MixBridge local WDK toolset shim (kits already installed). -->
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="`$(VCTargetsPath)\Platforms\`$(Platform)\PlatformToolsets\v143\Toolset.props"
          Condition="Exists('`$(VCTargetsPath)\Platforms\`$(Platform)\PlatformToolsets\v143\Toolset.props')" />
  <PropertyGroup>
    <PlatformToolsetShortName>WindowsKernelModeDriver10.0</PlatformToolsetShortName>
    <IsKernelModeToolset>true</IsKernelModeToolset>
    <IsUserModeToolset>false</IsUserModeToolset>
    <WDKContentRoot Condition="'`$(WDKContentRoot)' == ''">$WdkRoot</WDKContentRoot>
    <WindowsSdkDir Condition="'`$(WindowsSdkDir)' == ''">`$(WDKContentRoot)</WindowsSdkDir>
    <WindowsTargetPlatformVersion Condition="'`$(WindowsTargetPlatformVersion)' == ''">$WdkVersion</WindowsTargetPlatformVersion>
    <TargetPlatformVersion>`$(WindowsTargetPlatformVersion)</TargetPlatformVersion>
    <WDKBuildFolder Condition="'`$(WDKBuildFolder)' == ''">`$(WindowsTargetPlatformVersion)</WDKBuildFolder>
    <TargetVersion Condition="'`$(TargetVersion)' == ''">Windows10</TargetVersion>
    <DDKPlatform Condition="'`$(Platform)' == 'Win32'">x86</DDKPlatform>
    <DDKPlatform Condition="'`$(Platform)' == 'x64'">x64</DDKPlatform>
    <DDKPlatform Condition="'`$(Platform)' == 'ARM64'">ARM64</DDKPlatform>
    <DDKPlatform Condition="'`$(Platform)' == 'ARM64EC'">ARM64EC</DDKPlatform>
    <KMDF_VERSION_MAJOR Condition="'`$(KMDF_VERSION_MAJOR)' == ''">1</KMDF_VERSION_MAJOR>
    <KMDF_VERSION_MINOR Condition="'`$(KMDF_VERSION_MINOR)' == ''">15</KMDF_VERSION_MINOR>
  </PropertyGroup>
  <Import Project="`$(WDKContentRoot)build\`$(WDKBuildFolder)\WindowsDriver.Default.props"
          Condition="Exists('`$(WDKContentRoot)build\`$(WDKBuildFolder)\WindowsDriver.Default.props')" />
  <Import Project="`$(WDKContentRoot)DesignTime\CommonConfiguration\Neutral\WDK\`$(WindowsTargetPlatformVersion)\WDK.props"
          Condition="Exists('`$(WDKContentRoot)DesignTime\CommonConfiguration\Neutral\WDK\`$(WindowsTargetPlatformVersion)\WDK.props')" />
</Project>
"@

$targets = @"
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="`$(VCTargetsPath)\Platforms\`$(Platform)\PlatformToolsets\v143\Toolset.targets"
          Condition="Exists('`$(VCTargetsPath)\Platforms\`$(Platform)\PlatformToolsets\v143\Toolset.targets')" />
  <Import Project="`$(WDKContentRoot)build\`$(WDKBuildFolder)\WindowsDriver.Common.targets"
          Condition="Exists('`$(WDKContentRoot)build\`$(WDKBuildFolder)\WindowsDriver.Common.targets')" />
</Project>
"@

$platforms = @('x64', 'Win32', 'ARM64')
foreach ($platform in $platforms) {
    $toolsetDir = Join-Path $vc "Platforms\$platform\PlatformToolsets\WindowsKernelModeDriver10.0"
    New-Item -ItemType Directory -Force -Path $toolsetDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $toolsetDir 'ImportAfter') | Out-Null

    Set-Content -Path (Join-Path $toolsetDir 'Toolset.props') -Value $props -Encoding UTF8
    Set-Content -Path (Join-Path $toolsetDir 'Toolset.targets') -Value $targets -Encoding UTF8

    $importAfterDir = Join-Path $vc "Platforms\$platform\ImportAfter"
    New-Item -ItemType Directory -Force -Path $importAfterDir | Out-Null
    $src = Join-Path $WdkRoot "build\$WdkVersion\$platform\ImportAfter"
    if (Test-Path $src) {
        Copy-Item -Path (Join-Path $src '*') -Destination $importAfterDir -Force
    }

    $kitToolset = Join-Path $WdkRoot "build\$WdkVersion\$platform\WindowsKernelModeDriver"
    if (Test-Path $kitToolset) {
        Copy-Item -Path (Join-Path $kitToolset '*') -Destination (Join-Path $toolsetDir 'ImportAfter') -Force
    }
}

# Also install into BuildTools if present.
$bt = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
if ((Test-Path $bt) -and ($bt -ne $VsInstance)) {
    & $PSCommandPath -VsInstance $bt -WdkRoot $WdkRoot.TrimEnd('\') -WdkVersion $WdkVersion
}

Write-Host "Installed WindowsKernelModeDriver10.0 under $vc"
Test-Path (Join-Path $vc 'Platforms\x64\PlatformToolsets\WindowsKernelModeDriver10.0\Toolset.props')
