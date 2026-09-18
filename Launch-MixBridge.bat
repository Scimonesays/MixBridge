@echo off
setlocal
cd /d "%~dp0"

set "RELEASE_EXE=%~dp0apps\desktop\src-tauri\target\release\mixbridge-desktop.exe"
set "DEBUG_EXE=%~dp0apps\desktop\src-tauri\target\debug\mixbridge-desktop.exe"

if exist "%RELEASE_EXE%" (
  start "" "%RELEASE_EXE%"
  exit /b 0
)

if exist "%DEBUG_EXE%" (
  start "" "%DEBUG_EXE%"
  exit /b 0
)

cd /d "%~dp0apps\desktop"
if not exist "node_modules\" (
  echo Installing desktop dependencies...
  call npm install
  if errorlevel 1 (
    echo npm install failed.
    pause
    exit /b 1
  )
)

echo Starting MixBridge ^(tauri dev^)...
call npm run tauri -- dev
if errorlevel 1 (
  echo MixBridge failed to start.
  pause
  exit /b 1
)
endlocal
