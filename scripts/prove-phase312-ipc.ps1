# Prove live engine control path used by the desktop shell (Phase 3.12 IPC contract).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "native\audio-engine\build"
$Ipc = Join-Path $Build "mb-engine-ipc.exe"
$Out = Join-Path $Root "artifacts\qa\latest\logs"
New-Item -ItemType Directory -Force -Path $Out | Out-Null

if (-not (Test-Path $Ipc)) { throw "Missing $Ipc" }

Get-Process mb-engine-ipc -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$proc = Start-Process -FilePath $Ipc -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 1

function Invoke-Mb([string]$cmd) {
  $code = @"
`$n = New-Object System.IO.Pipes.NamedPipeClientStream('.', 'mixbridge-engine', [System.IO.Pipes.PipeDirection]::InOut)
`$n.Connect(5000)
`$r = New-Object System.IO.StreamReader(`$n)
`$w = New-Object System.IO.StreamWriter(`$n); `$w.AutoFlush = `$true
`$null = `$r.ReadLine()
`$w.WriteLine('$cmd')
`$resp = `$r.ReadLine()
Write-Output `$resp
`$n.Dispose()
"@
  return (powershell -NoProfile -Command $code).Trim()
}

function Get-Field([string]$line, [string]$key) {
  $parts = $line -split "\s+"
  $i = [array]::IndexOf($parts, $key)
  if ($i -lt 0 -or $i + 1 -ge $parts.Count) { return $null }
  return $parts[$i + 1]
}

$script:failures = 0
function Check([bool]$cond, [string]$msg) {
  if ($cond) { Write-Host "PASS: $msg" }
  else {
    Write-Host "FAIL: $msg"
    $script:failures++
  }
}

Check ((Invoke-Mb "PING") -match "PONG") "connect/PING"

$idLine = Invoke-Mb "ADD_TONE 440"
Check ($idLine -match "OK ID") "add tone"
$id = Get-Field $idLine "ID"
Check ($null -ne $id -and $id -ne "") "parsed tone id=$id"

$start = Invoke-Mb "START"
Check ($start -match "OK STARTED") "start engine"
Start-Sleep -Milliseconds 1000

# Drain one meter window, then sample
Invoke-Mb "METER_MASTER" | Out-Null
Start-Sleep -Milliseconds 500
$meter1 = Invoke-Mb "METER_MASTER"
$peak1 = [double](Get-Field $meter1 "PEAK")
$rms1 = [double](Get-Field $meter1 "RMS")
Check ($peak1 -gt 0.02 -or $rms1 -gt 0.01) "live meter responds (peak=$peak1 rms=$rms1)"

$g = Invoke-Mb "SET_GAIN $id 0.2"
Check ($g -match "^OK") "set gain 0.2 ($g)"
Start-Sleep -Milliseconds 400
Invoke-Mb "METER_MASTER" | Out-Null
Start-Sleep -Milliseconds 600
$meterLow = Invoke-Mb "METER_MASTER"
$peakLow = [double](Get-Field $meterLow "PEAK")
$rmsLow = [double](Get-Field $meterLow "RMS")
Check ($rmsLow -lt $rms1 * 0.55) "gain down reduces RMS (rmsLow=$rmsLow rms1=$rms1 peakLow=$peakLow)"

$g2 = Invoke-Mb "SET_GAIN $id 1.0"
Check ($g2 -match "^OK") "restore gain"
$m = Invoke-Mb "SET_MUTE $id 1"
Check ($m -match "^OK") "mute on"
Start-Sleep -Milliseconds 400
Invoke-Mb "METER_MASTER" | Out-Null
Start-Sleep -Milliseconds 600
$meterMute = Invoke-Mb "METER_MASTER"
$peakMute = [double](Get-Field $meterMute "PEAK")
$rmsMute = [double](Get-Field $meterMute "RMS")
Check ($peakMute -lt 0.01 -and $rmsMute -lt 0.005) "mute silences meter (peak=$peakMute rms=$rmsMute)"

Invoke-Mb "SET_MUTE $id 0" | Out-Null
$restart = Invoke-Mb "RESTART"
Check ($restart -match "OK RESTARTED") "engine restart"
Start-Sleep -Milliseconds 800
$status = Invoke-Mb "STATUS"
Check ($status -match "STATE running") "running after restart"

# IPC disappearance recovery
Stop-Process -Id $proc.Id -Force
Start-Sleep -Milliseconds 500
$proc = Start-Process -FilePath $Ipc -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 1
Check ((Invoke-Mb "PING") -match "PONG") "recover after IPC disappear"

Invoke-Mb "SHUTDOWN" | Out-Null
Start-Sleep -Milliseconds 400
if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }

@"
# Phase 3.12 IPC live-control proof
Date: $(Get-Date -Format o)
failures: $($script:failures)
peak1=$peak1 rms1=$rms1 peakLow=$peakLow rmsLow=$rmsLow peakMute=$peakMute rmsMute=$rmsMute
"@ | Set-Content -Encoding utf8 (Join-Path $Out "phase312-ipc-proof.txt")

Write-Host "failures=$($script:failures)"
if ($script:failures -gt 0) { exit 1 } else { Write-Host "phase312_ipc_result=PASS"; exit 0 }
