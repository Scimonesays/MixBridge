$ErrorActionPreference = "Continue"
$log = "C:\Users\scimo\Documents\CODE\MixBridge\artifacts\qa\latest\hotswap-driver.log"
function L($m){ Add-Content $log (("[{0}] {1}" -f (Get-Date -Format o), $m)); Write-Host $m }
Remove-Item $log -Force -EA SilentlyContinue
L "hotswap start — no machine reboot"

$devcon = "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"
$inf = "C:\Users\scimo\Documents\CODE\MixBridge\native\driver\x64\Release\package\MixBridgeDriver.inf"
$pkg = "C:\Users\scimo\Documents\CODE\MixBridge\native\driver\x64\Release\package"

# Stop audio stack so .sys unlocks
Stop-Service Audiosrv -Force -ErrorAction SilentlyContinue
Stop-Service AudioEndpointBuilder -Force -ErrorAction SilentlyContinue
Start-Sleep 2

& $devcon remove "ROOT\MixBridgeDriver" 2>&1 | ForEach-Object { L "remove: $_" }
sc.exe stop MixBridgeDriver 2>&1 | ForEach-Object { L "$_" }
Start-Sleep 1

# Ensure package is signed fresh
& "C:\Users\scimo\Documents\CODE\MixBridge\native\driver\scripts\install-testsign.ps1" 2>&1 | ForEach-Object { L "$_" }

Start-Sleep 1
Start-Service AudioEndpointBuilder -ErrorAction SilentlyContinue
Start-Service Audiosrv -ErrorAction SilentlyContinue
Start-Sleep 3

Get-PnpDevice | ? { $_.FriendlyName -match "MixBridge" } | ForEach-Object { L "PNP $($_.Status) $($_.Problem) $($_.FriendlyName) $($_.InstanceId)" }
$sys = Get-ChildItem "C:\Windows\System32\DriverStore\FileRepository" -Recurse -Filter "mixbridgedriver.sys" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
L "store sys=$($sys.FullName) time=$($sys.LastWriteTime) size=$($sys.Length)"
L "pkg time=$((Get-Item "$pkg\MixBridgeDriver.sys").LastWriteTime) size=$((Get-Item "$pkg\MixBridgeDriver.sys").Length)"
L "hotswap done"
