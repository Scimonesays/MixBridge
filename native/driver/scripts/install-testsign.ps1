#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Install MixBridge driver package for local development (test signing).

.DESCRIPTION
  Creates a test code-signing certificate, signs the built .sys + .cat, and
  installs via pnputil. Requires elevated PowerShell and test-signing enabled
  (see README).

.PARAMETER PackageDir
  Directory containing MixBridgeDriver.sys and MixBridgeDriver.inf
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

$sys = Get-ChildItem $PackageDir -Filter '*.sys' | Select-Object -First 1
$inf = Get-ChildItem $PackageDir -Filter '*.inf' | Select-Object -First 1
if (-not $sys -or -not $inf) {
    throw "Missing .sys/.inf in $PackageDir — build the driver first (scripts/build.ps1)."
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
        -KeyUsage DigitalSignature -CertStoreLocation Cert:\LocalMachine\My `
        -KeyExportPolicy Exportable -HashAlgorithm SHA256
    $cerPath = Join-Path $PackageDir 'MixBridgeTest.cer'
    Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null
} else {
    # Ensure trust roots present for install
    $cerPath = Join-Path $PackageDir 'MixBridgeTest.cer'
    if (-not (Test-Path $cerPath)) {
        Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null
    }
    if (-not (Get-ChildItem Cert:\LocalMachine\Root | Where-Object { $_.Thumbprint -eq $cert.Thumbprint })) {
        Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
    }
    if (-not (Get-ChildItem Cert:\LocalMachine\TrustedPublisher | Where-Object { $_.Thumbprint -eq $cert.Thumbprint })) {
        Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null
    }
}

$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter 'signtool.exe' |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $signtool) { throw 'signtool.exe not found in Windows Kits.' }

$inf2cat = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter 'Inf2Cat.exe' |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $inf2cat) { throw 'Inf2Cat.exe not found in Windows Kits.' }

function Invoke-Sign([string] $Path) {
    Write-Host "Signing $Path ..."
    & $signtool.FullName sign /v /fd SHA256 /sm /s My /n $certName /sha1 $cert.Thumbprint $Path
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed for $Path (exit $LASTEXITCODE)"
    }
}

Invoke-Sign $sys.FullName

Write-Host "Generating catalog with Inf2Cat ..."
& $inf2cat.FullName /driver:$PackageDir /os:10_X64,Server10_X64,10_VU_X64,Server10_VU_X64,10_NI_X64,Server10_NI_X64,10_GE_X64,Server10_GE_X64
if ($LASTEXITCODE -ne 0) {
    # Fallback to broader Win10 x64 family token used by older kits
    & $inf2cat.FullName /driver:$PackageDir /os:10_X64
    if ($LASTEXITCODE -ne 0) {
        throw "Inf2Cat failed (exit $LASTEXITCODE)"
    }
}

$cat = Get-ChildItem $PackageDir -Filter '*.cat' | Select-Object -First 1
if (-not $cat) { throw "Inf2Cat did not produce a .cat in $PackageDir" }
Invoke-Sign $cat.FullName

Write-Host "Installing driver package from $PackageDir ..."
pnputil /add-driver $inf.FullName /install
if ($LASTEXITCODE -ne 0) {
    throw "pnputil /add-driver failed (exit $LASTEXITCODE)"
}

$devcon = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Tools" -Recurse -Filter 'devcon.exe' |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if ($devcon) {
    Write-Host "Creating ROOT\MixBridgeDriver device node ..."
    & $devcon.FullName remove 'ROOT\MixBridgeDriver' 2>$null | Out-Null
    & $devcon.FullName install $inf.FullName 'ROOT\MixBridgeDriver'
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "devcon install returned $LASTEXITCODE — check Device Manager for MixBridge Virtual Audio."
    }
} else {
    Write-Warning 'devcon.exe not found; package is in the store but ROOT device was not created.'
}

$hvci = 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity'
if ((Test-Path $hvci) -and ((Get-ItemProperty $hvci -Name Enabled -ErrorAction SilentlyContinue).Enabled -eq 1)) {
    Write-Warning 'Memory Integrity (HVCI) is ON and blocks test-signed drivers. Disabling for MixBridge load (reboot required).'
    Set-ItemProperty -Path $hvci -Name Enabled -Value 0 -Type DWord
}

Write-Host 'Done. Verify endpoints in Sound settings:'
Write-Host '  Playback  → Speakers (MixBridge Virtual Audio)  [MixBridge Input]'
Write-Host '  Recording → Microphone Array (MixBridge Virtual Audio)  [MixBridge Output]'
Write-Host 'If the device shows Code 52, reboot after disabling Memory Integrity / with testsigning ON.'
