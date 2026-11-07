"""Open MixBridge Output (capture) and measure peak while Live is playing."""
import time
import sys

def main():
    try:
        import comtypes
        from comtypes import CLSCTX_ALL
        from pycaw.pycaw import AudioUtilities, IAudioMeterInformation
    except Exception as e:
        print("IMPORT_FAIL", e)
        return 2

    targets = []
    for d in AudioUtilities.GetAllDevices():
        name = getattr(d, "FriendlyName", None) or ""
        if "MixBridge" in name:
            targets.append(d)
            print("FOUND", name, getattr(d, "id", ""))

    if not targets:
        print("NO_MIXBRIDGE_DEVICES")
        return 1

    for d in targets:
        name = d.FriendlyName
        try:
            meter = d._dev.Activate(IAudioMeterInformation._iid_, CLSCTX_ALL, None)
            meter = comtypes.cast(meter, comtypes.POINTER(IAudioMeterInformation))
            peaks = []
            for _ in range(40):
                peaks.append(float(meter.GetPeakValue()))
                time.sleep(0.05)
            print(f"METER {name}: max={max(peaks):.6f} avg={sum(peaks)/len(peaks):.6f}")
        except Exception as e:
            print(f"METER_FAIL {name}: {e}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
