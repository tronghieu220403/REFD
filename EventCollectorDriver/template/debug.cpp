#include "debug.h"
#include <Ntstrsafe.h>
#include <wdm.h>
#include <stdarg.h>
#include "../std/ulti/def.h"

static HANDLE g_LogFileHandle = NULL;
static LONG g_LogWrittenCount = 0;

typedef struct _LOG_SLOT {
    PWCHAR Buffer;      // allocated buffer (WCHAR*)
    ULONG  Bytes;       // bytes to write (not include extra null)
} LOG_SLOT, * PLOG_SLOT;

static LOG_SLOT* g_Arr1 = nullptr;
static LOG_SLOT* g_Arr2 = nullptr;

// active/work pointers
static LOG_SLOT* g_ArrWaitWrite = nullptr;
static LOG_SLOT* g_ArrWrite = nullptr;

static volatile LONG g_ActiveCount = 0;
static volatile LONG g_WorkCount = 0;

// spinlock to protect swap + active push index
static KSPIN_LOCK g_LogArrLock;

// Worker thread state
static HANDLE     g_LogThreadHandle = NULL;
static volatile BOOLEAN g_LogThreadStop = FALSE;

static __forceinline LONGLONG GetNtSystemTime100ns()
{
    LARGE_INTEGER st;
    KeQuerySystemTime(&st);
    return st.QuadPart; // 100ns since 1601
}

VOID initLogFile();
VOID closeLogFile();
VOID writeToLogFile(PVOID buffer, ULONG size);

VOID initLogFile() {

    if (g_LogFileHandle != NULL) {
        return;
    }

    //DebugMessage("Open g_LogFileHandle");

    UNICODE_STRING fileName;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;

    RtlInitUnicodeString(&fileName, LOG_PATH);
    InitializeObjectAttributes(
        &attr,
        &fileName,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    NTSTATUS status = ZwCreateFile(
        &g_LogFileHandle,
        FILE_APPEND_DATA | SYNCHRONIZE,
        &attr,
        &io,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN_IF,
        FILE_WRITE_THROUGH | FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY | FILE_NON_DIRECTORY_FILE,  // Do not FILE_SYNCHRONOUS_* because deadlock
        NULL,
        0
    );

    if (!NT_SUCCESS(status)) {
        g_LogFileHandle = NULL;
    }
    InterlockedExchange(&g_LogWrittenCount, 0);
}

VOID closeLogFile() {
    if (g_LogFileHandle) {
        ZwClose(g_LogFileHandle);
        g_LogFileHandle = NULL;
    }
}

VOID writeToLogFile(PVOID buffer, ULONG size)
{
    if (g_LogFileHandle == nullptr) {
        initLogFile();
    }
    if (g_LogFileHandle == nullptr) {
        closeLogFile();
        return;
    }
    //
    // Rotate log when threshold reached
    //
    InterlockedIncrement(&g_LogWrittenCount);
    if (g_LogWrittenCount >= DEBUG_LOG_THRESHOLD) {
        closeLogFile();
        initLogFile();
    }
    if (g_LogFileHandle == nullptr) {
        return;
    }

    IO_STATUS_BLOCK io = { 0 };

    LARGE_INTEGER li_byte_offset;
    li_byte_offset.HighPart = -1;
    li_byte_offset.LowPart = FILE_WRITE_TO_END_OF_FILE;

    //DebugMessage("ZwWriteFile begin (%p, %d)", buffer, size);

    auto status = ZwWriteFile(g_LogFileHandle,
        NULL,
        NULL, NULL,
        &io,
        buffer,
        size,
        NULL,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        //DebugMessage("ZwWriteFile (%p, %d) failed, status %x", buffer, size, status);
    }
    else {
        //DebugMessage("ZwWriteFile (%p, %d) succeed", buffer, size);
    }
    return;
}

static __forceinline VOID freeWorkArrayEntries()
{
    LONG cnt = g_WorkCount;
    if (cnt <= 0) return;

    for (LONG i = 0; i < cnt; i++) {
        if (g_ArrWrite[i].Buffer) {
            ExFreePoolWithTag(g_ArrWrite[i].Buffer, LOG_TAG);
            g_ArrWrite[i].Buffer = nullptr;
            g_ArrWrite[i].Bytes = 0;
        }
    }
    g_WorkCount = 0;
}

//
// Worker thread body: runs at PASSIVE_LEVEL, writes queued logs to file
//
VOID LogWorkerThread(_In_ PVOID start_context)
{
    UNREFERENCED_PARAMETER(start_context);

    DebugMessage("DebugLogWorkerThread started");
    LONGLONG lastFlush = GetNtSystemTime100ns();

    for (;;) {

        if (g_LogThreadStop) {
            DebugMessage("g_LogThreadStop == true, break");
            break;
        }

        // Check flush conditions
        bool doFlush = false;
        LONGLONG now = GetNtSystemTime100ns();

        // time condition
        if (now - lastFlush >= (LONGLONG)LOG_FLUSH_SEC * 10LL * 1000LL * 1000LL) {
            doFlush = true;
        }

        // threshold condition
        if (g_ActiveCount > LOG_SWAP_TH) {
            doFlush = true;
        }

        if (doFlush) {
            // swap arrays quickly
            KIRQL oldIrql;
            KeAcquireSpinLock(&g_LogArrLock, &oldIrql);

            // Move active -> work by swapping pointers
            LOG_SLOT* tmp = g_ArrWaitWrite;
            g_ArrWaitWrite = g_ArrWrite;
            g_ArrWrite = tmp;

            // WorkCount becomes previous ActiveCount
            g_WorkCount = g_ActiveCount;
            g_ActiveCount = 0;

            KeReleaseSpinLock(&g_LogArrLock, oldIrql);

            if (g_WorkCount <= 0) {
                continue;
            }

            // Compute total bytes
            SIZE_T totalBytes = 0;
            for (LONG i = 0; i < g_WorkCount; i++) {
                totalBytes += g_ArrWrite[i].Bytes;
            }

            if (totalBytes == 0) {
                freeWorkArrayEntries();
                continue;
            }

            // Allocate one big buffer and copy all
            PUCHAR big = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, totalBytes, LOG_TAG);
            SIZE_T off = 0;
            if (big) {
                for (LONG i = 0; i < g_WorkCount; i++) {
                    if (g_ArrWrite[i].Buffer && g_ArrWrite[i].Bytes) {
                        RtlCopyMemory(big + off, g_ArrWrite[i].Buffer, g_ArrWrite[i].Bytes);
                        off += g_ArrWrite[i].Bytes;
                    }
                }
            }

            // free per-entry buffers
            freeWorkArrayEntries();

            if (big != nullptr) {
                writeToLogFile(big, off);
                ExFreePoolWithTag(big, LOG_TAG);
            }

            lastFlush = now;
        }

        LARGE_INTEGER interval;
        interval.QuadPart = -10 * 1000 * 10;  // 100ms
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }

    DebugMessage("DebugLogWorkerThread exiting");
    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID PushToLogQueue(PCWSTR fmt, ...)
{
    if (!fmt ||
        KeGetCurrentIrql() != PASSIVE_LEVEL) {
        return;
    }

    NTSTATUS status = STATUS_SUCCESS;

    PWCHAR p_log = (PWCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, INITIAL_LOG_ALLOC * sizeof(WCHAR), LOG_TAG);
    if (p_log == nullptr) {
        return;
    }
    RtlZeroMemory(p_log, INITIAL_LOG_ALLOC * sizeof(WCHAR));
    size_t remaining = 0;

    //
    // Try formatting into the buffer
    // STRSAFE_NO_TRUNCATION ensures failure instead of silent truncation
    //
    va_list args;
    va_start(args, fmt);
    status = RtlStringCbVPrintfExW(
        p_log, (INITIAL_LOG_ALLOC - 1) * sizeof(WCHAR),
        NULL, &remaining, STRSAFE_NO_TRUNCATION,
        fmt, args
    );
    va_end(args);

    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(p_log, LOG_TAG);
        return;
    }
    ULONG sz = wcsnlen(p_log, INITIAL_LOG_ALLOC - 1) * sizeof(WCHAR);

    if (sz == 0) {
        ExFreePoolWithTag(p_log, LOG_TAG);
        return;
    }

    DebugMessage("%ws", p_log);

    // push into active array
    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&g_LogArrLock, &oldIrql);

    LONG idx = g_ActiveCount;
    if (idx >= LOG_ARRAY_CAP) {
        KeReleaseSpinLock(&g_LogArrLock, oldIrql);
        ExFreePoolWithTag(p_log, LOG_TAG);
        p_log = nullptr;
        return;
    }

    g_ArrWaitWrite[idx].Buffer = p_log;
    g_ArrWaitWrite[idx].Bytes = sz;
    g_ActiveCount = idx + 1;

    KeReleaseSpinLock(&g_LogArrLock, oldIrql);
    return;
}

//
// Initialize logging system: queue + event + worker thread
//
NTSTATUS InitDebugSystem()
{
    DebugMessage("Begin InitDebugSystem");

    NTSTATUS status;

    // Initialize underlying log file object
    initLogFile();

    // allocate two arrays upfront
    g_Arr1 = (LOG_SLOT*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(LOG_SLOT) * LOG_ARRAY_CAP, LOG_TAG);
    g_Arr2 = (LOG_SLOT*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(LOG_SLOT) * LOG_ARRAY_CAP, LOG_TAG);
    if (!g_Arr1 || !g_Arr2) {
        if (g_Arr1) ExFreePoolWithTag(g_Arr1, LOG_TAG);
        if (g_Arr2) ExFreePoolWithTag(g_Arr2, LOG_TAG);
        g_Arr1 = g_Arr2 = nullptr;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_Arr1, sizeof(LOG_SLOT) * LOG_ARRAY_CAP);
    RtlZeroMemory(g_Arr2, sizeof(LOG_SLOT) * LOG_ARRAY_CAP);

    KeInitializeSpinLock(&g_LogArrLock);

    g_ArrWaitWrite = g_Arr1;
    g_ArrWrite = g_Arr2;
    g_ActiveCount = 0;
    g_WorkCount = 0;

    g_LogThreadStop = FALSE;
    g_LogThreadHandle = NULL;

    // Create worker thread
    status = PsCreateSystemThread(
        &g_LogThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        LogWorkerThread,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        DebugMessage("PsCreateSystemThread failed: 0x%08X", status);

        // free arrays (no per-entry buffers yet, so safe)
        ExFreePoolWithTag(g_Arr1, LOG_TAG);
        ExFreePoolWithTag(g_Arr2, LOG_TAG);
        g_Arr1 = g_Arr2 = nullptr;
        g_ArrWaitWrite = g_ArrWrite = nullptr;

        g_LogThreadHandle = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

//
// Stop worker thread and cleanup queue + file
//
VOID CloseDebugSystem()
{
    DebugMessage("Begin");
    // Signal worker thread to stop
    g_LogThreadStop = TRUE;

    // Wait for worker to exit and close handle
    if (g_LogThreadHandle) {
        ZwWaitForSingleObject(g_LogThreadHandle, FALSE, NULL);
        ZwClose(g_LogThreadHandle);
        g_LogThreadHandle = NULL;
    }

    // Free any remaining buffers in both arrays
    // We need to safely snapshot counts + pointers
    {
        KIRQL oldIrql = 0;
        KeAcquireSpinLock(&g_LogArrLock, &oldIrql);

        // Free active entries
        LONG ac = g_ActiveCount;
        LOG_SLOT* ap = g_ArrWaitWrite;

        // Free work entries
        LONG wc = g_WorkCount;
        LOG_SLOT* wp = g_ArrWrite;

        // reset counts to avoid double free if called again
        g_ActiveCount = 0;
        g_WorkCount = 0;

        KeReleaseSpinLock(&g_LogArrLock, oldIrql);

        for (LONG i = 0; i < ac; i++) {
            if (ap[i].Buffer) {
                ExFreePoolWithTag(ap[i].Buffer, LOG_TAG);
                ap[i].Buffer = nullptr;
                ap[i].Bytes = 0;
            }
        }

        for (LONG i = 0; i < wc; i++) {
            if (wp[i].Buffer) {
                ExFreePoolWithTag(wp[i].Buffer, LOG_TAG);
                wp[i].Buffer = nullptr;
                wp[i].Bytes = 0;
            }
        }
    }

    // Free arrays
    if (g_Arr1) {
        ExFreePoolWithTag(g_Arr1, LOG_TAG);
        g_Arr1 = nullptr;
    }
    if (g_Arr2) {
        ExFreePoolWithTag(g_Arr2, LOG_TAG);
        g_Arr2 = nullptr;
    }

    g_ArrWaitWrite = g_ArrWrite = nullptr;

    // Close file handle
    closeLogFile();
}