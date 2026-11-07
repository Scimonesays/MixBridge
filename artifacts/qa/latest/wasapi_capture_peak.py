"""Record ~1s from MixBridge Microphone Array and print PCM peak."""
import ctypes
import ctypes.wintypes as wt
import time
import uuid
from ctypes import POINTER, byref, cast, POINTER as P

ole32 = ctypes.oledll.ole32
ole32.CoInitialize(None)

CLSCTX_ALL = 23

class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.c_uint32),
        ("Data2", ctypes.c_uint16),
        ("Data3", ctypes.c_uint16),
        ("Data4", ctypes.c_ubyte * 8),
    ]

    @classmethod
    def from_uuid(cls, s):
        u = uuid.UUID(s)
        g = cls()
        g.Data1 = u.fields[0]
        g.Data2 = u.fields[1]
        g.Data3 = u.fields[2]
        b = u.bytes
        for i in range(8):
            g.Data4[i] = b[8 + i]
        return g

class WAVEFORMATEX(ctypes.Structure):
    _fields_ = [
        ("wFormatTag", ctypes.c_uint16),
        ("nChannels", ctypes.c_uint16),
        ("nSamplesPerSec", ctypes.c_uint32),
        ("nAvgBytesPerSec", ctypes.c_uint32),
        ("nBlockAlign", ctypes.c_uint16),
        ("wBitsPerSample", ctypes.c_uint16),
        ("cbSize", ctypes.c_uint16),
    ]

# Minimal vtable callers via comtypes
from comtypes import COMMETHOD, HRESULT, GUID as CGUID, IUnknown, CoCreateInstance, CLSCTX_ALL as CA
from comtypes import POINTER

class IMMDevice(IUnknown):
    _iid_ = CGUID("{D666063F-1587-4E43-81F1-B948E807363F}")
    _methods_ = [
        COMMETHOD([], HRESULT, "Activate",
                  (["in"], CGUID, "iid"),
                  (["in"], ctypes.c_uint32, "dwClsCtx"),
                  (["in"], ctypes.c_void_p, "pActivationParams"),
                  (["out"], POINTER(ctypes.c_void_p), "ppInterface")),
        COMMETHOD([], HRESULT, "OpenPropertyStore"),
        COMMETHOD([], HRESULT, "GetId", (["out"], POINTER(ctypes.c_wchar_p), "ppstrId")),
        COMMETHOD([], HRESULT, "GetState", (["out"], POINTER(ctypes.c_uint32), "pdwState")),
    ]

class IMMDeviceEnumerator(IUnknown):
    _iid_ = CGUID("{A95664D2-9614-4F35-A746-DE8DB63617E6}")
    _methods_ = [
        COMMETHOD([], HRESULT, "EnumAudioEndpoints"),
        COMMETHOD([], HRESULT, "GetDefaultAudioEndpoint"),
        COMMETHOD([], HRESULT, "GetDevice",
                  (["in"], ctypes.c_wchar_p, "pwstrId"),
                  (["out"], POINTER(POINTER(IMMDevice)), "ppDevice")),
        COMMETHOD([], HRESULT, "RegisterEndpointNotificationCallback"),
        COMMETHOD([], HRESULT, "UnregisterEndpointNotificationCallback"),
    ]

class IAudioClient(IUnknown):
    _iid_ = CGUID("{1CB9AD4C-DBFA-4c32-B178-C2F568A703B2}")
    _methods_ = [
        COMMETHOD([], HRESULT, "Initialize",
                  (["in"], ctypes.c_int, "ShareMode"),
                  (["in"], ctypes.c_uint32, "StreamFlags"),
                  (["in"], ctypes.c_int64, "hnsBufferDuration"),
                  (["in"], ctypes.c_int64, "hnsPeriodicity"),
                  (["in"], ctypes.c_void_p, "pFormat"),
                  (["in"], ctypes.c_void_p, "AudioSessionGuid")),
        COMMETHOD([], HRESULT, "GetBufferSize", (["out"], POINTER(ctypes.c_uint32), "pNumBufferFrames")),
        COMMETHOD([], HRESULT, "GetStreamLatency", (["out"], POINTER(ctypes.c_int64), "phnsLatency")),
        COMMETHOD([], HRESULT, "GetCurrentPadding", (["out"], POINTER(ctypes.c_uint32), "pNumPaddingFrames")),
        COMMETHOD([], HRESULT, "IsFormatSupported"),
        COMMETHOD([], HRESULT, "GetMixFormat", (["out"], POINTER(ctypes.c_void_p), "ppDeviceFormat")),
        COMMETHOD([], HRESULT, "GetDevicePeriod"),
        COMMETHOD([], HRESULT, "Start"),
        COMMETHOD([], HRESULT, "Stop"),
        COMMETHOD([], HRESULT, "Reset"),
        COMMETHOD([], HRESULT, "SetEventHandle"),
        COMMETHOD([], HRESULT, "GetService",
                  (["in"], CGUID, "riid"),
                  (["out"], POINTER(ctypes.c_void_p), "ppv")),
    ]

class IAudioCaptureClient(IUnknown):
    _iid_ = CGUID("{C8ADCCF7-B26E-4962-A7ED-5DBA8C7A7A4A}")
    _methods_ = [
        COMMETHOD([], HRESULT, "GetBuffer",
                  (["out"], POINTER(ctypes.c_void_p), "ppData"),
                  (["out"], POINTER(ctypes.c_uint32), "pNumFramesToRead"),
                  (["out"], POINTER(ctypes.c_uint32), "pdwFlags"),
                  (["out"], POINTER(ctypes.c_uint64), "pu64DevicePosition"),
                  (["out"], POINTER(ctypes.c_uint64), "pu64QPCPosition")),
        COMMETHOD([], HRESULT, "ReleaseBuffer", (["in"], ctypes.c_uint32, "NumFramesRead")),
        COMMETHOD([], HRESULT, "GetNextPacketSize", (["out"], POINTER(ctypes.c_uint32), "pNumFramesInNextPacket")),
    ]

CLSID = CGUID("{BCDE0395-E52F-467C-8E3D-C4579291692E}")
enumerator = CoCreateInstance(CLSID, IMMDeviceEnumerator, CA)

cap_id = "{0.0.1.00000000}.{2f818797-c1d9-489c-bf70-397357bb8c69}"
device = enumerator.GetDevice(cap_id)

punk = ctypes.c_void_p()
# Activate IAudioClient
hr = device.Activate(IAudioClient._iid_, CA, None)
# comtypes Activate returns interface when using typed helper - redo properly

from comtypes import client
# Use pycaw device activate pattern
from pycaw.pycaw import AudioUtilities

dev = None
for d in AudioUtilities.GetAllDevices():
    if d and d.id and cap_id.lower() in d.id.lower():
        dev = d
        break
if not dev:
    # try match name
    for d in AudioUtilities.GetAllDevices():
        if d and d.FriendlyName and "Microphone Array (MixBridge" in d.FriendlyName:
            dev = d
            break
assert dev, "capture device not found"
print("Using", dev.FriendlyName, dev.id)

audio_client = ctypes.cast(
    dev._dev.Activate(IAudioClient._iid_, CA, None),
    POINTER(IAudioClient),
)

mix = ctypes.c_void_p()
audio_client.GetMixFormat(byref(mix))
fmt = cast(mix, POINTER(WAVEFORMATEX)).contents
print(f"mix format: ch={fmt.nChannels} rate={fmt.nSamplesPerSec} bits={fmt.wBitsPerSample} tag={fmt.wFormatTag} align={fmt.nBlockAlign}")

# shared mode, 1s buffer
AUDCLNT_SHAREMODE_SHARED = 0
hr = audio_client.Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, mix, None)
print("Initialize hr", hr)

svc = ctypes.c_void_p()
audio_client.GetService(IAudioCaptureClient._iid_, byref(svc))
cap = ctypes.cast(svc, POINTER(IAudioCaptureClient))
audio_client.Start()

peak = 0.0
frames_total = 0
silent_packets = 0
data_packets = 0
end = time.time() + 1.5
while time.time() < end:
    nxt = ctypes.c_uint32(0)
    cap.GetNextPacketSize(byref(nxt))
    while nxt.value:
        data = ctypes.c_void_p()
        nframes = ctypes.c_uint32()
        flags = ctypes.c_uint32()
        a = ctypes.c_uint64(); b = ctypes.c_uint64()
        cap.GetBuffer(byref(data), byref(nframes), byref(flags), byref(a), byref(b))
        n = nframes.value
        if flags.value & 0x2:  # AUDCLNT_BUFFERFLAGS_SILENT
            silent_packets += 1
        elif data and n:
            data_packets += 1
            # float32 mix format typical
            count = n * fmt.nChannels
            arr = (ctypes.c_float * count).from_address(data.value)
            for i in range(count):
                v = abs(arr[i])
                if v > peak:
                    peak = v
            frames_total += n
        cap.ReleaseBuffer(n)
        cap.GetNextPacketSize(byref(nxt))
    time.sleep(0.005)

audio_client.Stop()
print(f"RESULT peak={peak:.6f} frames={frames_total} data_packets={data_packets} silent_packets={silent_packets}")
