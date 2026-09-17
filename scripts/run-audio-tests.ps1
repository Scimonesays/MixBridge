# Run deterministic DSP tests + WASAPI probe selftest.
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "native\probes\build"
$Out = Join-Path $Root "artifacts\qa\latest"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Out "logs") | Out-Null

function Require-Exe([string]$name) {
  $p = Join-Path $Build $name
  if (-not (Test-Path $p)) {
    throw "Missing $p - run scripts/build-probes.ps1 first"
  }
  return $p
}

$analyze = Require-Exe "mb-pcm-analyze.exe"
$probe = Require-Exe "mb-audio-probe.exe"
Write-Host "analyze=$analyze"
Write-Host "probe=$probe"

Write-Host "=== DSP unit tests (ctest) ==="
Push-Location $Build
ctest --output-on-failure
$ctestRc = $LASTEXITCODE
Pop-Location

Write-Host "=== WASAPI probe selftest ==="
& $probe selftest 2>&1 | Tee-Object -FilePath (Join-Path $Out "logs\probe-selftest.txt")
$probeRc = $LASTEXITCODE

$summary = @"
# MixBridge QA - probe phase

Date: $(Get-Date -Format o)
ctest_exit: $ctestRc
probe_selftest_exit: $probeRc

## Notes
DSP tests are deterministic and hardware-independent.
WASAPI selftest exercises enumerate/render/system-loopback/process-loopback.
Capture without a microphone is soft-warned, not always a hard failure.
Do not mark Gate 3-7 PASS from this script alone.
"@
Set-Content -Encoding utf8 (Join-Path $Out "SUMMARY.md") $summary
Write-Host $summary
exit ([Math]::Max($ctestRc, $probeRc))
