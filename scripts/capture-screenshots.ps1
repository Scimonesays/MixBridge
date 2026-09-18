# Launch MixBridge desktop (release preferred) and prepare the final screenshot folder.
# Does not auto-drive UI states — follow artifacts/qa/final/screenshots/README.md for the 7 shots.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ShotDir = Join-Path $Root "artifacts\qa\final\screenshots"
$Readme = Join-Path $ShotDir "README.md"
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null

$candidates = @(
  (Join-Path $Root "apps\desktop\src-tauri\target\release\mixbridge-desktop.exe"),
  (Join-Path $env:LOCALAPPDATA "MixBridge\mixbridge-desktop.exe"),
  (Join-Path ${env:ProgramFiles} "MixBridge\mixbridge-desktop.exe")
)

$exe = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$running = Get-Process -Name "mixbridge-desktop" -ErrorAction SilentlyContinue

if ($running) {
  Write-Host "MixBridge already running (PID $($running.Id -join ', '))."
} elseif ($exe) {
  Write-Host "Launching: $exe"
  Start-Process -FilePath $exe | Out-Null
  Start-Sleep -Seconds 2
} else {
  Write-Host "No release desktop exe found. Build first:"
  Write-Host "  cd apps\desktop"
  Write-Host "  npm run tauri -- build"
  Write-Host "Or run: npm run tauri -- dev"
}

Write-Host ""
Write-Host "Screenshot folder: $ShotDir"
Write-Host "Checklist:       $Readme"
Write-Host ""
Write-Host "Capture these PNGs (product doctrine):"
Write-Host "  01-standby.png"
Write-Host "  02-source-picker.png"
Write-Host "  03-multi-source.png"
Write-Host "  04-vst-guitar.png"
Write-Host "  05-on-air.png"
Write-Host "  06-builtin-instrument.png"
Write-Host "  07-advanced-diag.png"
Write-Host ""
Write-Host "Then run the visual review checklist in the README before flipping Y/Z gates."

if (Test-Path $ShotDir) {
  Start-Process explorer.exe $ShotDir | Out-Null
}
