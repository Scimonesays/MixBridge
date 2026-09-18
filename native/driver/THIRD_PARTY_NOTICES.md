# Third-party notices — MixBridge virtual audio driver

MixBridge `native/driver/` adapts structural patterns from:

1. **Virtual-Audio-Driver** (MIT) — MikeTheTech / VirtualDrivers  
   Reference clone: `research/upstream/Virtual-Audio-Driver` (gitignored, not a build dependency)

2. **Microsoft Windows Driver Samples — SysVAD / Simple Audio Sample** (MS-PL)  
   Original PortCls / WaveRT miniport layout. MixBridge reimplements under MIT with attribution; MS-PL source is **not** copied into product libraries.

MixBridge driver sources in this tree are **MIT** (see repository root `LICENSE`). Do not merge MS-PL sample code into MIT-only crates or the audio engine.
