' Starts MixBridge with no console window. Falls back to a visible dev shell
' only when the desktop executable has not been built yet.
Option Explicit

Dim fso, shell, root, releaseExe, debugExe
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
root = fso.GetParentFolderName(WScript.ScriptFullName)
releaseExe = root & "\apps\desktop\src-tauri\target\release\mixbridge-desktop.exe"
debugExe = root & "\apps\desktop\src-tauri\target\debug\mixbridge-desktop.exe"

If fso.FileExists(releaseExe) Then
  shell.Run """" & releaseExe & """", 0, False
ElseIf fso.FileExists(debugExe) Then
  shell.Run """" & debugExe & """", 0, False
Else
  shell.CurrentDirectory = root & "\apps\desktop"
  shell.Run "cmd /c npm run tauri -- dev", 1, False
End If
