$ErrorActionPreference = "Continue"
$log = "C:\Users\scimo\Documents\CODE\MixBridge\artifacts\qa\latest\reinstall-cable-fix.log"
function L($m){ Add-Content $log (("[{0}] {1}" -f (Get-Date -Format o), $m)) }
Remove-Item $log -Force -EA SilentlyContinue
L "reinstall cable fix (no reboot)"
# Do NOT touch HVCI or shutdown
& "C:\Users\scimo\Documents\CODE\MixBridge\native\driver\scripts\install-testsign.ps1" 2>&1 | ForEach-Object { L "$_" }
pnputil /restart-device "ROOT\MEDIA\0000" 2>&1 | ForEach-Object { L "$_" }
Start-Sleep 2
Get-PnpDevice | ? { $_.FriendlyName -match "MixBridge" } | ForEach-Object { L "$($_.Status) $($_.Problem) $($_.FriendlyName)" }
L "done"
