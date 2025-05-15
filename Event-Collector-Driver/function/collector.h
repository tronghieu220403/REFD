﻿#pragma once

#ifndef COLLECTOR_H
#define COLLECTOR_H
#include <fltKernel.h>
#include "../../std/string/string.h"
#include "../../std/vector/vector.h"
#include "../../std/map/map.h"
#include "../../std/sync/mutex.h"
#include "../../template/register.h"
#include "../../template/flt-ex.h"
#include "detector.h"

namespace collector
{
    typedef struct _HANDLE_CONTEXT
    {
		ULONG requestor_pid = 0;
		bool is_modified = false;
		bool is_deleted = false;
		bool is_created = false;
        bool is_renamed = false;
        WCHAR current_path[HIEUNT_MAX_PATH] = { 0 };
        WCHAR new_path[HIEUNT_MAX_PATH] = { 0 };
        WCHAR backup_name[HIEUNT_MAX_PATH] = { 0 };
    } HANDLE_CONTEXT, * PHANDLE_CONTEXT;

    void DrvRegister();
    void DrvUnload();
    void FltRegister();
    void FltUnload();

    FLT_PREOP_CALLBACK_STATUS PreDoNothing(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);

    FLT_POSTOP_CALLBACK_STATUS PostFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileWrite(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

} // namespace

#endif // COLLECTOR_H
