/*++
  MixBridge Input → Output soft-buffer cable (VB-Cable replacement).
  Render streams push PCM into this ring; capture streams pull it out.
--*/

#ifndef _MIXBRIDGE_CABLE_RING_H_
#define _MIXBRIDGE_CABLE_RING_H_

#include "definitions.h"

// ~100 ms of stereo 24-bit @ 48 kHz ≈ 28800 bytes; size for 192 kHz / 24-bit headroom.
#define MIXBRIDGE_CABLE_RING_BYTES  (192000u * 2u * 3u / 10u)  // 100 ms @ max rate

NTSTATUS
MixBridgeCableRingInitialize(void);

VOID
MixBridgeCableRingTeardown(void);

// Copy from render DMA into the cable (drops oldest on overflow).
VOID
MixBridgeCableRingWrite(
    _In_reads_bytes_(ByteCount) const BYTE* Buffer,
    _In_ ULONG ByteCount
);

// Fill capture DMA from the cable; zeros any shortfall.
VOID
MixBridgeCableRingRead(
    _Out_writes_bytes_(ByteCount) BYTE* Buffer,
    _In_ ULONG ByteCount
);

#endif // _MIXBRIDGE_CABLE_RING_H_
