#pragma once

#ifndef COLLECTOR_H
#define COLLECTOR_H
#include <fltKernel.h>
#include "../../std/string/wstring.h"
#include "../../std/vector/vector.h"
#include "../../std/map/map.h"
#include "../../std/sync/mutex.h"
#include "../../template/register.h"
#include "../../template/flt-ex.h"

#define HIEUNT_MAX_PATH 1024

namespace collector
{
    typedef struct _HANDLE_CONTEXT
    {
        ULONG requestor_pid = 0;
        std::WString process_path;

        std::WString path;

        LONGLONG file_size = 0;

        bool is_read = false;
        bool is_modified = false;

        LONGLONG first_write_timestamp = 0;
        LONGLONG last_write_timestamp = 0;
        LONGLONG write_cnt_bytes = 0;
        LONGLONG write_cnt_times = 0;
        double write_entropy = 0.0;

        LONGLONG first_read_timestamp = 0;
        LONGLONG last_read_timestamp = 0;
        LONGLONG read_cnt_bytes = 0;
        LONGLONG read_cnt_times = 0;
        double read_entropy = 0.0;

        bool is_renamed = false;
        std::WString new_path;

        bool is_created = false;
        
        bool is_deleted = false;
        
        bool is_alloc = false;
        
        bool is_eof = false;
        
        bool is_mmap_open = false;
        bool is_mmap_modified = false;
    } HANDLE_CONTEXT, * PHANDLE_CONTEXT;

    void DrvRegister();
    void DrvUnload();
    void FltRegister();
	NTSTATUS FltUnload();

    void ProcessNotifyCallback(PEPROCESS process, HANDLE pid, PPS_CREATE_NOTIFY_INFO create_info);

    FLT_PREOP_CALLBACK_STATUS PreFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreReadFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);

    FLT_PREOP_CALLBACK_STATUS PreFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);
    
    FLT_PREOP_CALLBACK_STATUS PreFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context);

    FLT_POSTOP_CALLBACK_STATUS PostFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileWrite(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostReadFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    FLT_POSTOP_CALLBACK_STATUS PostFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags);

    void ContextCleanup(PFLT_CONTEXT context, FLT_CONTEXT_TYPE context_type);

} // namespace

#endif // COLLECTOR_H
