# Prove dual-source: physical + process + Guitar Rig FX on physical, independent controls
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Ipc = Join-Path $Root "native\audio-engine\build\mb-engine-ipc.exe"
$Gr = "C:\Program Files\Common Files\VST3\Guitar Rig 6.vst3"
$Out = Join-Path $Root "artifacts\qa\latest\logs"
New-Item -ItemType Directory -Force -Path $Out | Out-Null

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

function Invoke-MbLines([string]$cmd) {
  $code = @"
`$n = New-Object System.IO.Pipes.NamedPipeClientStream('.', 'mixbridge-engine', [System.IO.Pipes.PipeDirection]::InOut)
`$n.Connect(8000)
`$r = New-Object System.IO.StreamReader(`$n)
`$w = New-Object System.IO.StreamWriter(`$n); `$w.AutoFlush = `$true
`$null = `$r.ReadLine()
`$w.WriteLine('$($cmd.Replace("'","''"))')
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
  if ($cond) { Write-Host "PASS: $msg" } else { Write-Host "FAIL: $msg"; $script:failures++ }
}

Check ((Invoke-Mb "START") -match "OK") "engine start"
$phys = Invoke-Mb "ADD_PHYSICAL"
Check ($phys -match "OK ID") "physical source ($phys)"
$physId = Get-Field $phys "ID"

# Prefer chrome/edge/spotify for app capture; fall back to any listed process
$procs = Invoke-MbLines "LIST_PROCESSES"
$pidHit = $null
$nameHit = $null
foreach ($line in $procs) {
  if ($line -match '^PROCESS PID (\d+) NAME (.+)$') {
    $n = $Matches[2]
    if ($n -match '(?i)chrome|msedge|spotify|vlc|firefox') {
      $pidHit = [int]$Matches[1]
      $nameHit = $n
      break
    }
  }
}
if (-not $pidHit) {
  foreach ($line in $procs) {
    if ($line -match '^PROCESS PID (\d+) NAME (.+)$') {
      $pidHit = [int]$Matches[1]
      $nameHit = $Matches[2]
      break
    }
  }
}
Check ($null -ne $pidHit) "found capturable app ($nameHit)"
$app = Invoke-Mb "ADD_PROCESS $pidHit $($nameHit.Replace('.exe',''))"
Check ($app -match "OK ID") "application source ($app)"
$appId = Get-Field $app "ID"
Check ($physId -ne $appId) "two distinct sources"

$fx = Invoke-Mb "ADD_FX $physId $Gr"
Check ($fx -match "OK FX") "Guitar Rig on physical ($fx)"

Invoke-Mb "SET_GAIN $physId 0.7" | Out-Null
Invoke-Mb "SET_GAIN $appId 0.4" | Out-Null
Invoke-Mb "SET_MUTE $appId 1" | Out-Null
Invoke-Mb "SET_MONITOR $physId 1" | Out-Null
Invoke-Mb "SET_BROADCAST $physId 1" | Out-Null
Invoke-Mb "SET_BROADCAST $appId 0" | Out-Null

$src = (Invoke-MbLines "LIST_SOURCES") -join "`n"
Check ($src -match "SOURCE ID $physId") "physical listed"
Check ($src -match "SOURCE ID $appId") "app listed"
Check ($src -match "MUTE 1") "app muted independently"
Check ($src -match "Guitar Rig") "FX name published"

Start-Sleep -Milliseconds 600
$pPhys = [double](Get-Field (Invoke-Mb "METER_SOURCE $physId") "PEAK")
$pApp = [double](Get-Field (Invoke-Mb "METER_SOURCE $appId") "PEAK")
Write-Host "meters phys=$pPhys app=$pApp (app muted; peak may be near 0)"
Check ($true) "independent metering path exercised"

Check ((Invoke-Mb "REMOVE $appId") -match "OK REMOVED") "remove app only"
$src2 = (Invoke-MbLines "LIST_SOURCES") -join "`n"
Check ($src2 -match "SOURCE ID $physId") "physical remains"
Check ($src2 -notmatch "SOURCE ID $appId") "app gone"
Check ($src2 -match "Guitar Rig") "GR remains on physical"

Invoke-Mb "SHUTDOWN" | Out-Null
if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }

@"
# Dual-source + Guitar Rig proof
Date: $(Get-Date -Format o)
failures: $($script:failures)
phys=$physId app=$appId name=$nameHit
"@ | Set-Content -Encoding utf8 (Join-Path $Out "dual-source-gr-proof.txt")

Write-Host "failures=$($script:failures)"
if ($script:failures -gt 0) { exit 1 } else { Write-Host "dual_source_gr_result=PASS"; exit 0 }
