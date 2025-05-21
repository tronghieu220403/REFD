#pragma once
#ifndef MINIFS_H
#define MINIFS_H
#include <fltKernel.h>
#include <dontuse.h>
#include "register.h"
#include "debug.h"
#include "system_routine.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

extern inline PFLT_FILTER kFilterHandle = nullptr;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT driver_object,
    _In_ PUNICODE_STRING registry_path
);

NTSTATUS 
DriverUnload(
    PDRIVER_OBJECT driver_object
);

NTSTATUS
MiniFsInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _In_ FLT_INSTANCE_SETUP_FLAGS flags,
    _In_ DEVICE_TYPE volume_device_type,
    _In_ FLT_FILESYSTEM_TYPE volume_filesystem_type
);

VOID
MiniFsInstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS flags
);

VOID
MiniFsInstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS flags
);

NTSTATUS
MiniFsUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS flags
);

NTSTATUS
MiniFsInstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS flags
);

FLT_PREOP_CALLBACK_STATUS
MiniFsPreOperation(
    _Inout_ PFLT_CALLBACK_DATA data,
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _Flt_CompletionContext_Outptr_ PVOID* completion_context
);

FLT_POSTOP_CALLBACK_STATUS
MiniFsPostOperation(
    _Inout_ PFLT_CALLBACK_DATA data,
    _In_ PCFLT_RELATED_OBJECTS flt_objects,
    _In_opt_ PVOID completion_context,
    _In_ FLT_POST_OPERATION_FLAGS flags
);

void MiniFsContextCleanup(PFLT_CONTEXT context, FLT_CONTEXT_TYPE context_type);

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, MiniFsUnload)
#pragma alloc_text(PAGE, MiniFsInstanceQueryTeardown)
#pragma alloc_text(PAGE, MiniFsInstanceSetup)
#pragma alloc_text(PAGE, MiniFsInstanceTeardownStart)
#pragma alloc_text(PAGE, MiniFsInstanceTeardownComplete)
#endif


#endif // MINIFS_H