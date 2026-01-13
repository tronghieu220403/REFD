#pragma once

#define HIEU_DEBUG

#ifdef HIEU_DEBUG

//#define DebugMessage(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#define DebugMessage(x, ...) DbgPrintEx(0, 0, "[EventCollectorDriver] [%s:%d] " x "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#else 

#define DebugMessage(x, ...) 

#endif // DEBUG

#include <fltKernel.h>

#pragma warning( disable : 4083 4024 4047 4702 4189 4101 4100)

#define LOG_PATH   L"\\SystemRoot\\EventCollectorDriver.log"
#define DEBUG_LOG_THRESHOLD 100
#define INITIAL_LOG_ALLOC 4096

#define LOG_TAG         'gLbD'
#define LOG_ARRAY_CAP   5000
#define LOG_SWAP_TH     2500
#define LOG_FLUSH_SEC   1

NTSTATUS InitDebugSystem();
VOID CloseDebugSystem();
VOID LogWorkerThread(_In_ PVOID start_context);
VOID PushToLogQueue(PCWSTR fmt, ...);

#ifdef HIEU_DEBUG

#define DebugMessage(x, ...) DbgPrintEx(0, 0, "[EventCollectorDriver] [%s:%d] " x "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#else 


#endif // DEBUG
