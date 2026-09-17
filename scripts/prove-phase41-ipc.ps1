# Phase 4.1: engine vs broadcast separation + real source IPC
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

function Invoke-MbLines([string]$cmd) {
  $code = @"
`$n = New-Object System.IO.Pipes.NamedPipeClientStream('.', 'mixbridge-engine', [System.IO.Pipes.PipeDirection]::InOut)
`$n.Connect(5000)
`$r = New-Object System.IO.StreamReader(`$n)
`$w = New-Object System.IO.StreamWriter(`$n); `$w.AutoFlush = `$true
`$null = `$r.ReadLine()
`$w.WriteLine('$cmd')
`$lines = New-Object System.Collections.Generic.List[string]
while (`$true) {
  `$resp = `$r.ReadLine()
  if (-not `$resp) { break }
  `$lines.Add(`$resp)
  if (`$resp -eq 'OK END' -or `$resp.StartsWith('ERR ')) { break }
}
`$lines -join "`n"
`$n.Dispose()
"@
  return (powershell -NoProfile -Command $code)
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

$status = Invoke-Mb "STATUS"
Check ($status -match "BROADCAST standby") "status includes broadcast standby"
Check ($status -match "LIVE_DEST 0") "no fake live destination"

$start = Invoke-Mb "START"
Check ($start -match "OK STARTED") "engine start (monitor)"

$live = Invoke-Mb "BROADCAST_ENABLE"
Check ($live -match "no_live_destination") "Go Live blocked without live destination"

$status2 = Invoke-Mb "STATUS"
Check ($status2 -match "STATE running") "engine still running in Standby"
Check ($status2 -match "BROADCAST standby") "broadcast remains standby"

$cap = Invoke-MbLines "LIST_CAPTURE"
Check ($cap -match "DEVICE ID ") "list capture includes IDs"
Check ($cap -match "NAME ") "list capture includes names"

$idA = Get-Field (Invoke-Mb "ADD_TONE 440") "ID"
$idB = Get-Field (Invoke-Mb "ADD_TONE 1000") "ID"
Check ($null -ne $idA -and $null -ne $idB -and $idA -ne $idB) "two tone sources for remove proof"

Invoke-Mb "SET_GAIN $idA 0.4" | Out-Null
Invoke-Mb "SET_MUTE $idB 1" | Out-Null
$rm = Invoke-Mb "REMOVE $idA"
Check ($rm -match "OK REMOVED") "remove source A"

$sources = Invoke-MbLines "LIST_SOURCES"
Check ($sources -match "SOURCE ID $idB") "B remains"
Check ($sources -notmatch "SOURCE ID $idA") "A gone"
Check ($sources -match "MUTE 1") "B mute preserved"

# Physical default capture
$phys = Invoke-Mb "ADD_PHYSICAL"
Check ($phys -match "OK ID") "add physical default capture ($phys)"
$physId = Get-Field $phys "ID"

$rm2 = Invoke-Mb "REMOVE $physId"
Check ($rm2 -match "OK REMOVED") "remove physical"

Invoke-Mb "BROADCAST_DISABLE" | Out-Null
# Engine stays up — stop is diagnostics-only path
$status3 = Invoke-Mb "STATUS"
Check ($status3 -match "STATE running") "engine still running after broadcast disable"

Invoke-Mb "SHUTDOWN" | Out-Null
Start-Sleep -Milliseconds 400
if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }

@"
# Phase 4.1 IPC proof
Date: $(Get-Date -Format o)
failures: $($script:failures)
"@ | Set-Content -Encoding utf8 (Join-Path $Out "phase41-ipc-proof.txt")

Write-Host "failures=$($script:failures)"
if ($script:failures -gt 0) { exit 1 } else { Write-Host "phase41_ipc_result=PASS"; exit 0 }
