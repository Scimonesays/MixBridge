/*++
  MixBridge Input → Output soft-buffer cable.
--*/

#include "cable_ring.h"

namespace {

KSPIN_LOCK g_CableLock;
BYTE* g_CableBuffer = nullptr;
ULONG g_CableSize = 0;
ULONG g_CableWrite = 0;
ULONG g_CableRead = 0;
ULONG g_CableFilled = 0;
BOOLEAN g_CableReady = FALSE;

} // namespace

#pragma code_seg("PAGE")
NTSTATUS
MixBridgeCableRingInitialize(void)
{
    PAGED_CODE();

    if (g_CableReady) {
        return STATUS_SUCCESS;
    }

    KeInitializeSpinLock(&g_CableLock);
    g_CableSize = MIXBRIDGE_CABLE_RING_BYTES;
    g_CableBuffer = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, g_CableSize, MIXBRIDGE_POOLTAG);
    if (!g_CableBuffer) {
        g_CableSize = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_CableWrite = 0;
    g_CableRead = 0;
    g_CableFilled = 0;
    g_CableReady = TRUE;
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
VOID
MixBridgeCableRingTeardown(void)
{
    PAGED_CODE();

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_CableLock, &oldIrql);
    g_CableReady = FALSE;
    BYTE* buffer = g_CableBuffer;
    g_CableBuffer = nullptr;
    g_CableSize = 0;
    g_CableWrite = 0;
    g_CableRead = 0;
    g_CableFilled = 0;
    KeReleaseSpinLock(&g_CableLock, oldIrql);

    if (buffer) {
        ExFreePoolWithTag(buffer, MIXBRIDGE_POOLTAG);
    }
}

#pragma code_seg()
VOID
MixBridgeCableRingWrite(
    _In_reads_bytes_(ByteCount) const BYTE* Buffer,
    _In_ ULONG ByteCount
)
{
    if (!Buffer || ByteCount == 0) {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_CableLock, &oldIrql);
    if (!g_CableReady || !g_CableBuffer || g_CableSize == 0) {
        KeReleaseSpinLock(&g_CableLock, oldIrql);
        return;
    }

    ULONG remaining = ByteCount;
    ULONG srcOffset = 0;
    while (remaining > 0) {
        // Drop oldest bytes if the ring is full so Live never blocks.
        if (g_CableFilled >= g_CableSize) {
            ULONG drop = min(remaining, g_CableSize);
            g_CableRead = (g_CableRead + drop) % g_CableSize;
            g_CableFilled -= drop;
        }

        ULONG space = g_CableSize - g_CableFilled;
        ULONG run = min(remaining, space);
        run = min(run, g_CableSize - g_CableWrite);

        RtlCopyMemory(g_CableBuffer + g_CableWrite, Buffer + srcOffset, run);
        g_CableWrite = (g_CableWrite + run) % g_CableSize;
        g_CableFilled += run;
        srcOffset += run;
        remaining -= run;
    }

    KeReleaseSpinLock(&g_CableLock, oldIrql);
}

#pragma code_seg()
VOID
MixBridgeCableRingRead(
    _Out_writes_bytes_(ByteCount) BYTE* Buffer,
    _In_ ULONG ByteCount
)
{
    if (!Buffer || ByteCount == 0) {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_CableLock, &oldIrql);
    if (!g_CableReady || !g_CableBuffer || g_CableSize == 0) {
        KeReleaseSpinLock(&g_CableLock, oldIrql);
        RtlZeroMemory(Buffer, ByteCount);
        return;
    }

    ULONG remaining = ByteCount;
    ULONG dstOffset = 0;
    while (remaining > 0 && g_CableFilled > 0) {
        ULONG run = min(remaining, g_CableFilled);
        run = min(run, g_CableSize - g_CableRead);
        RtlCopyMemory(Buffer + dstOffset, g_CableBuffer + g_CableRead, run);
        g_CableRead = (g_CableRead + run) % g_CableSize;
        g_CableFilled -= run;
        dstOffset += run;
        remaining -= run;
    }

    KeReleaseSpinLock(&g_CableLock, oldIrql);

    if (remaining > 0) {
        RtlZeroMemory(Buffer + dstOffset, remaining);
    }
}
