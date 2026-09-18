#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Install MixBridge driver package for local development (test signing).

.DESCRIPTION
  Creates a test code-signing certificate, signs the built .sys, and installs
  via pnputil. Requires elevated PowerShell and test-signing enabled (see README).

  Does NOT disable Driver Signature Enforcement globally beyond the standard
  bcdedit testsigning ON workflow documented for kernel driver development.

.PARAMETER PackageDir
  Directory containing mixbridgedriver.sys and MixBridgeDriver.inf
  (default: native/driver/x64/Release/package relative to repo root).
#>
[CmdletBinding()]
param(
    [string] $PackageDir
)

$ErrorActionPreference = 'Stop'
$DriverRoot = Split-Path $PSScriptRoot -Parent
if (-not $PackageDir) {
    $PackageDir = Join-Path $DriverRoot 'x64\Release\package'
}

$sys = Join-Path $PackageDir 'mixbridgedriver.sys'
$inf = Join-Path $PackageDir 'MixBridgeDriver.inf'

foreach ($path in @($sys, $inf)) {
    if (-not (Test-Path $path)) {
        throw "Missing $path — build the driver first (scripts/build.ps1)."
    }
}

$testSigning = bcdedit /enum '{current}' | Select-String 'testsigning'
if ($testSigning -notmatch 'Yes') {
    Write-Warning 'testsigning is not ON. Enable with: bcdedit /set testsigning on (reboot required).'
}

$certName = 'MixBridge Test Driver Cert'
$cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -eq "CN=$certName" } | Select-Object -First 1
if (-not $cert) {
    Write-Host "Creating test certificate: $certName"
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=$certName" `
        -KeyUsage DigitalSignature -CertStoreLocation Cert:\LocalMachine\My
    Export-Certificate -Cert $cert -FilePath (Join-Path $PackageDir 'MixBridgeTest.cer') | Out-Null
    Import-Certificate -FilePath (Join-Path $PackageDir 'MixBridgeTest.cer') -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
}

$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter 'signtool.exe' |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1

if (-not $signtool) { throw 'signtool.exe not found in Windows Kits.' }

Write-Host "Signing $sys ..."
& $signtool.FullName sign /v /fd SHA256 /s My /n $certName /t http://timestamp.digicert.com $sys

Write-Host "Installing driver package from $PackageDir ..."
pnputil /add-driver $inf /install

Write-Host 'Done. Verify endpoints in Sound settings:'
Write-Host '  Playback  → MixBridge Input  (engine render target)'
Write-Host '  Recording → MixBridge Output (Discord / OBS mic source)'
