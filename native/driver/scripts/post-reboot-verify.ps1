#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  After reboot (testsigning + HVCI off), start MixBridge and record endpoint QA.
#>
$ErrorActionPreference = 'Continue'
$outDir = 'C:\Users\scimo\Documents\CODE\MixBridge\artifacts\qa\latest'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$log = Join-Path $outDir 'driver-post-reboot.log'
function L($m) { Add-Content $log (("[{0}] {1}" -f (Get-Date -Format o), $m)); Write-Host $m }

Remove-Item $log -Force -ErrorAction SilentlyContinue
L 'post-reboot verify start'
L ("testsigning: " + ((bcdedit /enum '{current}' | Select-String 'testsigning') -join ' '))
$hvci = 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity'
if (Test-Path $hvci) {
    L ("HVCI Enabled=" + (Get-ItemProperty $hvci).Enabled)
}

pnputil /restart-device 'ROOT\MEDIA\0000' 2>&1 | ForEach-Object { L $_ }
Start-Sleep -Seconds 3

$dev = Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -eq 'MixBridge Virtual Audio' }
if ($dev) {
    L ("device Status=$($dev.Status) Problem=$($dev.Problem) InstanceId=$($dev.InstanceId)")
} else {
    L 'MixBridge Virtual Audio device NOT FOUND'
    $devcon = 'C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe'
    $inf = 'C:\Users\scimo\Documents\CODE\MixBridge\native\driver\x64\Release\package\MixBridgeDriver.inf'
    if ((Test-Path $devcon) -and (Test-Path $inf)) {
        & $devcon install $inf 'ROOT\MixBridgeDriver' 2>&1 | ForEach-Object { L $_ }
        Start-Sleep -Seconds 3
        $dev = Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -eq 'MixBridge Virtual Audio' }
        if ($dev) { L ("device after reinstall Status=$($dev.Status) Problem=$($dev.Problem)") }
    }
}

$qa = [ordered]@{
    timestamp = (Get-Date).ToString('o')
    testsigning = ((bcdedit /enum '{current}' | Select-String 'testsigning') -join ' ').Trim()
    hvci = if (Test-Path $hvci) { (Get-ItemProperty $hvci).Enabled } else { $null }
    device_status = $dev.Status
    device_problem = "$($dev.Problem)"
    render = @()
    capture = @()
}

Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render' -ErrorAction SilentlyContinue | ForEach-Object {
    $name = (Get-ItemProperty "$($_.PSPath)\Properties" -ErrorAction SilentlyContinue).'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
    if ($name -match 'MixBridge') {
        $qa.render += @{ id = $_.PSChildName; name = $name }
        L "Render: $name ($($_.PSChildName))"
    }
}
Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture' -ErrorAction SilentlyContinue | ForEach-Object {
    $name = (Get-ItemProperty "$($_.PSPath)\Properties" -ErrorAction SilentlyContinue).'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
    if ($name -match 'MixBridge') {
        $qa.capture += @{ id = $_.PSChildName; name = $name }
        L "Capture: $name ($($_.PSChildName))"
    }
}

$qa | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $outDir 'mixbridge-endpoints.json') -Encoding UTF8
L 'wrote mixbridge-endpoints.json'
L 'post-reboot verify done'
