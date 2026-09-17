# Clone approved MixBridge upstream reference repositories (shallow).
# Trees land in research/upstream/ which is gitignored.

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Upstream = Join-Path $Root "research\upstream"
New-Item -ItemType Directory -Force -Path $Upstream | Out-Null

function Clone-Shallow {
  param(
    [string]$Url,
    [string]$Dir,
    [string]$Branch = ""
  )
  $dest = Join-Path $Upstream $Dir
  if (Test-Path (Join-Path $dest ".git")) {
    Write-Host "SKIP (exists): $Dir"
    Push-Location $dest
    git fetch --depth 1 origin 2>$null
    $hash = (git rev-parse HEAD).Trim()
    Pop-Location
    return $hash
  }
  Write-Host "CLONE: $Dir"
  if ($Branch) {
    git clone --depth 1 --branch $Branch $Url $dest
  } else {
    git clone --depth 1 $Url $dest
  }
  Push-Location $dest
  $hash = (git rev-parse HEAD).Trim()
  Pop-Location
  return $hash
}

$results = @()

$results += [pscustomobject]@{ Project = "Virtual-Soundboard-Audio-Windows-Mixer"; Hash = (Clone-Shallow "https://github.com/3godzinyL/Virtual-Soundboard-Audio-Windows-Mixer.git" "Virtual-Soundboard-Audio-Windows-Mixer") }
$results += [pscustomobject]@{ Project = "wasamix"; Hash = (Clone-Shallow "https://github.com/ytchenak/wasamix.git" "wasamix") }
$results += [pscustomobject]@{ Project = "Virtual-Audio-Driver"; Hash = (Clone-Shallow "https://github.com/VirtualDrivers/Virtual-Audio-Driver.git" "Virtual-Audio-Driver") }
$results += [pscustomobject]@{ Project = "cpal"; Hash = (Clone-Shallow "https://github.com/RustAudio/cpal.git" "cpal") }
$results += [pscustomobject]@{ Project = "vst3sdk"; Hash = (Clone-Shallow "https://github.com/steinbergmedia/vst3sdk.git" "vst3sdk") }
$results += [pscustomobject]@{ Project = "rust-vst3-host"; Hash = (Clone-Shallow "https://github.com/HelgeSverre/rust-vst3-host.git" "rust-vst3-host") }
$results += [pscustomobject]@{ Project = "truce-rack"; Hash = (Clone-Shallow "https://github.com/truce-audio/truce-rack.git" "truce-rack") }
$results += [pscustomobject]@{ Project = "jamulus"; Hash = (Clone-Shallow "https://github.com/jamulussoftware/jamulus.git" "jamulus") }
$results += [pscustomobject]@{ Project = "sonobus"; Hash = (Clone-Shallow "https://github.com/sonosaurus/sonobus.git" "sonobus") }

# Large Microsoft sample trees: shallow clone then note paths of interest
$results += [pscustomobject]@{ Project = "Windows-classic-samples"; Hash = (Clone-Shallow "https://github.com/microsoft/Windows-classic-samples.git" "Windows-classic-samples") }
$results += [pscustomobject]@{ Project = "Windows-driver-samples"; Hash = (Clone-Shallow "https://github.com/microsoft/Windows-driver-samples.git" "Windows-driver-samples") }

$manifestPath = Join-Path $Root "research\findings\upstream-commits.json"
$results | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 $manifestPath
Write-Host ""
Write-Host "Wrote $manifestPath"
$results | Format-Table -AutoSize
