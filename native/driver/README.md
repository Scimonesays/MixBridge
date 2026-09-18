# MixBridge virtual audio device track

## V1 application path

MixBridge V1 renders its broadcast bus to a **real Windows Live output selected by the user**. With a compatible installed virtual cable/loopback endpoint:

```text
MixBridge Live mix → virtual-cable render side → virtual-cable capture side → Discord
```

This path is implemented and is the supported V1 route.

## First-party endpoint goal

A later signed Windows capture endpoint named **MixBridge Output** would let Discord/OBS select MixBridge directly.

| Requirement | Status |
|---|---|
| Broadcast bus | Done |
| Real selected Live WASAPI output | Done |
| External virtual endpoint compatibility | Supported; not bundled |
| User-mode → kernel MixBridge audio transport | Not implemented |
| WDK build matrix | Not completed |
| Production/attestation signing | External prerequisite |
| Signed install/update/uninstall proof | Not run |
| Signed Discord/OBS capture proof | Not run |

A SysVAD-derived/open driver is not complete merely because it installs or is renamed. Actual MixBridge audio must reach the capture endpoint and the package must be production-signed.

The first-party driver is therefore a separately gated post-V1 deliverable.
