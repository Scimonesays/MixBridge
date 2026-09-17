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
while (`$true) {
  `$resp = `$r.ReadLine()
  if (-not `$resp) { break }
  Write-Output `$resp
  if (`$resp -eq 'OK END' -or `$resp.StartsWith('ERR ')) { break }
}
`$n.Dispose()
"@
  $raw = powershell -NoProfile -Command $code
  if ($null -eq $raw) { return @() }
  if ($raw -is [System.Array]) { return @($raw | ForEach-Object { "$_".Trim() }) }
  return @("$raw".Trim())
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

$liveBlocked = Invoke-Mb "BROADCAST_ENABLE"
Check ($liveBlocked -match "no_live_destination") "Go Live blocked without live destination"

$status2 = Invoke-Mb "STATUS"
Check ($status2 -match "STATE running") "engine still running in Standby"
Check ($status2 -match "BROADCAST standby") "broadcast remains standby"

$render = Invoke-MbLines "LIST_RENDER"
Check (($render | Where-Object { $_ -match "DEVICE ID " }).Count -gt 0) "list render includes IDs"
$renderId = $null
$renderName = $null
foreach ($line in $render) {
  if ($line.StartsWith("DEVICE ID ") -and $line.Contains(" NAME ")) {
    $rest = $line.Substring("DEVICE ID ".Length)
    $idx = $rest.IndexOf(" NAME ")
    if ($idx -gt 0) {
      $renderId = $rest.Substring(0, $idx)
      $renderName = $rest.Substring($idx + " NAME ".Length)
      break
    }
  }
}
Check ($null -ne $renderId -and $renderId.Length -gt 0) "parsed render device id ($renderName)"
$setLive = Invoke-Mb "SET_LIVE_DEVICE $renderId"
Check ($setLive -match "OK LIVE_DEST") "set live destination ($setLive)"
$goLive = Invoke-Mb "BROADCAST_ENABLE"
Check ($goLive -match "OK LIVE") "Go Live with real destination ($goLive)"
$statusLive = Invoke-Mb "STATUS"
Check ($statusLive -match "BROADCAST live") "broadcast live while engine running"
Check ($statusLive -match "STATE running") "engine still running On Air"
$standby = Invoke-Mb "BROADCAST_DISABLE"
Check ($standby -match "OK STANDBY") "return to Standby"
$statusSb = Invoke-Mb "STATUS"
Check ($statusSb -match "BROADCAST standby") "broadcast standby again"
Check ($statusSb -match "STATE running") "monitor continues after Standby"

$cap = Invoke-MbLines "LIST_CAPTURE"
Check (($cap | Where-Object { $_ -match "DEVICE ID " }).Count -gt 0) "list capture includes IDs"
Check (($cap | Where-Object { $_ -match "NAME " }).Count -gt 0) "list capture includes names"

$idA = Get-Field (Invoke-Mb "ADD_TONE 440") "ID"
$idB = Get-Field (Invoke-Mb "ADD_TONE 1000") "ID"
Check ($null -ne $idA -and $null -ne $idB -and $idA -ne $idB) "two tone sources for remove proof"

Invoke-Mb "SET_GAIN $idA 0.4" | Out-Null
Invoke-Mb "SET_MUTE $idB 1" | Out-Null
$rm = Invoke-Mb "REMOVE $idA"
Check ($rm -match "OK REMOVED") "remove source A"

$sources = Invoke-MbLines "LIST_SOURCES"
$srcText = ($sources -join "`n")
Check ($srcText -match "SOURCE ID $idB") "B remains"
Check ($srcText -notmatch "SOURCE ID $idA") "A gone"
Check ($srcText -match "MUTE 1") "B mute preserved"

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
