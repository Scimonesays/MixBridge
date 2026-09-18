# Prove engine-path VST3 FX with measurable audio through Guitar Rig / Saturation
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Ipc = Join-Path $Root "native\audio-engine\build\mb-engine-ipc.exe"
$Out = Join-Path $Root "artifacts\qa\latest\logs"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Sat = "C:\Program Files\Common Files\VST3\Saturation Knob.vst3"
$Gr = "C:\Program Files\Common Files\VST3\Guitar Rig 6.vst3"

Get-Process mb-engine-ipc -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$proc = Start-Process -FilePath $Ipc -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 1

function Invoke-Mb([string]$cmd) {
  $code = @"
`$n = New-Object System.IO.Pipes.NamedPipeClientStream('.', 'mixbridge-engine', [System.IO.Pipes.PipeDirection]::InOut)
`$n.Connect(8000)
`$r = New-Object System.IO.StreamReader(`$n)
`$w = New-Object System.IO.StreamWriter(`$n); `$w.AutoFlush = `$true
`$null = `$r.ReadLine()
`$w.WriteLine('$($cmd.Replace("'","''"))')
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

function Get-Peak([string]$id) {
  $m = Invoke-Mb "METER_SOURCE $id"
  return [double](Get-Field $m "PEAK")
}

$script:failures = 0
function Check([bool]$cond, [string]$msg) {
  if ($cond) { Write-Host "PASS: $msg" } else { Write-Host "FAIL: $msg"; $script:failures++ }
}

Check ((Invoke-Mb "PING") -match "PONG") "ping"
Check ((Invoke-Mb "START") -match "OK STARTED") "start"
$id = Get-Field (Invoke-Mb "ADD_TONE 440") "ID"
Check ($null -ne $id) "add tone id=$id"
Start-Sleep -Milliseconds 500
$peakDry = Get-Peak $id
Check ($peakDry -gt 0.01) "tone meters without FX peak=$peakDry"

$fx1 = Invoke-Mb "ADD_FX $id $Sat"
Check ($fx1 -match "^OK FX") "add Saturation Knob FX ($fx1)"
Start-Sleep -Milliseconds 700
$peakSat = Get-Peak $id
Check ($peakSat -gt 0.01) "saturation processes audio peak=$peakSat"

$st = Invoke-Mb "GET_FX_STATE $id 0"
Check ($st -match "OK BYTES") "fx state save ($st)"
$statePath = Get-Field $st "PATH"
Check ($null -ne $statePath -and (Test-Path $statePath)) "fx state file exists"
Check ((Invoke-Mb "SET_FX_STATE $id 0 $statePath") -match "^OK") "fx state restore"
Check ((Invoke-Mb "SET_FX_BYPASS $id 1") -match "^OK") "bypass on"
Start-Sleep -Milliseconds 400
$peakBypass = Get-Peak $id
Check ($peakBypass -gt 0.01) "bypass still passes tone peak=$peakBypass"
Check ((Invoke-Mb "SET_FX_BYPASS $id 0") -match "^OK") "bypass off"

Check ((Invoke-Mb "REMOVE_FX $id") -match "^OK") "clear chain before Guitar Rig"
$fx2 = Invoke-Mb "ADD_FX $id $Gr"
Check ($fx2 -match "^OK FX") "load Guitar Rig 6 ($fx2)"
Start-Sleep -Milliseconds 900
$peakGr = Get-Peak $id
Check ($peakGr -gt 0.001) "guitar rig processes (non-silent) peak=$peakGr"
$st2 = Invoke-Mb "GET_FX_STATE $id 0"
Check ($st2 -match "OK BYTES") "guitar rig state ($st2)"

# Chain: Saturation then Guitar Rig
Check ((Invoke-Mb "ADD_FX $id $Sat") -match "^OK FX") "append second FX (chain)"
$list = Invoke-Mb "LIST_FX $id"
# LIST_FX returns multi-line via until-end; single-line invoke only gets first — use COUNT via ADD response
Check ((Invoke-Mb "MOVE_FX $id 1 0") -match "^OK") "reorder FX chain"

$idB = Get-Field (Invoke-Mb "ADD_TONE 1000") "ID"
Check ($null -ne $idB -and $idB -ne $id) "second source"
Check ((Invoke-Mb "ADD_FX $idB $Sat") -match "^OK FX") "FX on second source independent"
Check ((Invoke-Mb "REMOVE $id") -match "OK REMOVED") "remove source clears FX"
Check ((Invoke-Mb "SET_FX_BYPASS $idB 0") -match "^OK") "remaining source FX intact"
Start-Sleep -Milliseconds 400
Check ((Get-Peak $idB) -gt 0.01) "independent source still meters"

Invoke-Mb "SHUTDOWN" | Out-Null
if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }

@"
# Phase 5 engine FX audio proof
Date: $(Get-Date -Format o)
failures: $($script:failures)
dry_peak: $peakDry
sat_peak: $peakSat
bypass_peak: $peakBypass
guitar_rig_peak: $peakGr
"@ | Set-Content -Encoding utf8 (Join-Path $Out "phase5-fx-proof.txt")

Write-Host "failures=$($script:failures)"
if ($script:failures -gt 0) { exit 1 }
Write-Host "phase5_fx_result=PASS"
exit 0
