# Build & run Phase 3 engine tools

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation

cmake -S native/audio-engine -B native/audio-engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build native/audio-engine/build
ctest --test-dir native/audio-engine/build --output-on-failure

# Integration (needs mb-tone-player from probes build)
native/audio-engine/build/mb-engine-harness.exe --seconds 2
native/audio-engine/build/mb-engine-soak.exe --minutes 2
native/audio-engine/build/mb-engine-ipc.exe
```
