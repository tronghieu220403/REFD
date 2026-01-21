#include "collector.h"
#include "../std/file/file.h"   
#include "../std/algo/entropy.h"
#include "../std/algo/hash.h"
#include "../template/common.h"
#include "../std/set/set.h"
#include "../std/sync/mutex.h"

namespace collector
{
    bool kIsProcessNotifyCallbackRegistered = false;
    bool kIsObCallbackRegistered = false;

    Set<ull>* sp = nullptr;
    Set<ull>* sf = nullptr;
    static Mutex mtx;

    // Register process and thread callbacks.
    void DrvRegister()
    {
        DebugMessage("%ws", __FUNCTIONW__);

        sp = new Set<ull>();
        sf = new Set<ull>();
        mtx.Create();

        // Register process creation and termination callback
        NTSTATUS status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, FALSE);
        if (!NT_SUCCESS(status))
        {
            DebugMessage("Fail to register process notify callback: %x", status);
        }
        else
        {
            DebugMessage("Process notify callback registered");
            kIsProcessNotifyCallbackRegistered = true;
        }
    }

    // Unregister process and thread callbacks.
    void DrvUnload()
    {
        DebugMessage("%ws", __FUNCTIONW__);

        if (kIsProcessNotifyCallbackRegistered == true)
        {
            PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, TRUE);
            kIsProcessNotifyCallbackRegistered = false;
        }

        delete sp;
        delete sf;
        sp = nullptr;
        sf = nullptr;
    }

    // Process notification callback
    void ProcessNotifyCallback(
        PEPROCESS process,
        HANDLE pid,
        PPS_CREATE_NOTIFY_INFO create_info
    )
    {
        const std::WString& process_path = GetProcessImageName(pid);
        if (create_info) {
            PushToLogQueue(L"P,%llu,%ws\n", (ull)pid, process_path.Data());
            mtx.Lock();
            sp->Insert((ull)pid);
            mtx.Unlock();
        }
        else {
            mtx.Lock();
            sp->Erase((ull)pid);
            mtx.Unlock();
        }
    }

    void FltRegister()
    {
        //DebugMessage("%ws", __FUNCTIONW__);

        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreFileCreate, PostFileCreate });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CLOSE, PreFileClose, PostFileClose });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CLEANUP, PreFileClose, PostFileClose });
        reg::kFltFuncVector->PushBack({ IRP_MJ_WRITE, PreWriteFile, PostFileWrite });
        reg::kFltFuncVector->PushBack({ IRP_MJ_READ, PreReadFile, PostReadFile });
        reg::kFltFuncVector->PushBack({ IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION, PreFileAcquireForSectionSync, PostFileAcquireForSectionSync });
        reg::kFltFuncVector->PushBack({ IRP_MJ_FILE_SYSTEM_CONTROL, PreFileSystemControl, PostFileSystemControl });
        reg::kFltFuncVector->PushBack({ IRP_MJ_SET_INFORMATION, PreFileSetInformation, PostFileSetInformation });

        //DebugMessage("Callbacks created.");
        return;
    }

    // Huỷ đăng ký các bộ lọc bảo vệ file
	NTSTATUS FltUnload()
    {
        //DebugMessage("Begin %ws", __FUNCTIONW__);
        /*
        std::WString file_path1(file::NormalizeDevicePathStr(L"\\??\\C:\\Users\\hieu\\Documents\\ggez.txt"));
        std::WString file_path2(file::NormalizeDevicePathStr(L"\\??\\E:\\hieunt210330\\ggez.txt"));
        if (file::ZwIsFileExist(file_path1) == true || file::ZwIsFileExist(file_path2) == true)
        {
            //DebugMessage("Magic files exist, so we allow the driver to unload");
            return STATUS_SUCCESS;
        }
        //DebugMessage("STATUS_FLT_DO_NOT_DETACH");
        return STATUS_FLT_DO_NOT_DETACH;
        */
        return STATUS_SUCCESS;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        //  Directory opens don't need to be scanned.
        if (FlagOn(data->Iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        //  Skip pre-rename operations which always open a directory.
        if (FlagOn(data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        //  Skip paging files.
        if (FlagOn(data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        //  Skip scanning DASD opens 
        if (FlagOn(flt_objects->FileObject->Flags, FO_VOLUME_OPEN))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        // Skip kernel mode
        if (data->RequestorMode == KernelMode) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        ULONG_PTR stack_low;
        ULONG_PTR stack_high;
        PFILE_OBJECT file_obj = data->Iopb->TargetFileObject;
        IoGetStackLimits(&stack_low, &stack_high);
        if (((ULONG_PTR)file_obj > stack_low) &&
            ((ULONG_PTR)file_obj < stack_high))
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        std::WString current_path(flt::GetFileFullPathName(data));
        if (current_path.Size() == 0 || current_path.HasCiSuffix(L"\\EventCollectorDriver.log")) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        //if (current_path.HasCiPrefix(L"\\device\\harddiskvolume3\\windows\\") == true
        //    || current_path.HasCiPrefix(L"\\device\\harddiskvolume3\\program files\\") == true
        //    || current_path.HasCiPrefix(L"\\device\\harddiskvolume3\\program files (x86)\\") == true
        //    || current_path.HasCiPrefix(L"\\device\\harddiskvolume3\\users\\hieu\\appdata\\") == true
        //    || current_path.HasCiPrefix(L"\\device\\harddiskvolume3\\programdata\\") == true
        //    ) {
        //    return FLT_PREOP_SUCCESS_NO_CALLBACK;
        //}

        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        UNREFERENCED_PARAMETER(completion_context);

        UNREFERENCED_PARAMETER(completion_context);

        if (!NT_SUCCESS(data->IoStatus.Status) ||
            (flags & FLTFL_POST_OPERATION_DRAINING) ||
            !FltSupportsStreamHandleContexts(flt_objects->FileObject)
            )
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        std::WString current_path = flt::GetFileFullPathName(data);

        if (current_path.Size() == 0) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        const auto& create_params = data->Iopb->Parameters.Create;

        bool is_created = (data->IoStatus.Information == FILE_CREATED);
        bool is_delete_on_close = FlagOn(create_params.Options, FILE_DELETE_ON_CLOSE);

        /*
        bool has_write_access = create_params.SecurityContext->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA);
        bool has_delete_access = create_params.SecurityContext->DesiredAccess & DELETE;
        // Not interested in files without write and delete access
        if (!is_created && !has_write_access && !is_delete_on_close && !has_delete_access)
        {
            DebugMessage("File: %ws, Not interested in files without create, write and delete access", current_path.Data());
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        */
        ULONG options = create_params.Options;

        PHANDLE_CONTEXT p_hc = nullptr;
        NTSTATUS status = FltAllocateContext(flt_objects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status)) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        memset(p_hc, 0, sizeof(HANDLE_CONTEXT));
        p_hc->requestor_pid = FltGetRequestorProcessId(data);
        p_hc->process_path = GetProcessImageName((HANDLE)p_hc->requestor_pid);
        p_hc->is_created = is_created;
        p_hc->is_deleted = is_delete_on_close;
        p_hc->path = current_path;

        LARGE_INTEGER li_file_size = { 0, 0 };
        FsRtlGetFileSize(flt_objects->FileObject, &li_file_size);
        p_hc->file_size = li_file_size.QuadPart;

        status = FltSetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(p_hc), nullptr);
        FltReleaseContext(p_hc);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        if (FsRtlIsPagingFile(flt_objects->FileObject) ||        // not interested in writes to the paging file 
            IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        PHANDLE_CONTEXT p_hc = nullptr;

        bool is_file_context = false;

        if (FlagOn(data->Iopb->IrpFlags, IRP_NOCACHE) && FlagOn(data->Iopb->IrpFlags, IRP_PAGING_IO))
            // If noncached paging I/O and not to the pagefile
        {
            // We do not ignore kernel mode writes here because this is where memory-mapped writes occur.
            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
            if (!NT_SUCCESS(status)) {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            p_hc->is_mmap_modified = true;
            FltDeleteContext(p_hc);
            FltReleaseContext(p_hc);
            // Not calculate entropy yet.
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (data->RequestorMode == KernelMode) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status)) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        defer(FltReleaseContext(p_hc););

        const auto& write_params = data->Iopb->Parameters.Write;
        UCHAR* buffer = nullptr;

        if (write_params.MdlAddress != nullptr) {
            buffer = (unsigned char*)MmGetSystemAddressForMdlSafe(write_params.MdlAddress,
                NormalPagePriority | MdlMappingNoExecute);
            if (buffer == nullptr) {
                data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                data->IoStatus.Information = 0;
                return FLT_PREOP_COMPLETE;
            }
        }
        else {
            __try {
                buffer = (UCHAR*)write_params.WriteBuffer;
                ProbeForRead(buffer, write_params.Length, 1);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                buffer = nullptr;
            }
        }
        if (buffer == nullptr) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        auto freq_write = new LONGLONG[256];
        RtlZeroMemory(freq_write, sizeof(LONGLONG) * 256);
        defer(delete[] freq_write;);

        ULONG length = write_params.Length;
        if (length == 0) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        __try {
            for (ULONG i = 0; i < length; i++) {
                ++freq_write[buffer[i]];
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        p_hc->is_modified = true;
        p_hc->write_cnt_bytes += length;
        auto write_entropy = math::ComputeByteEntropyFromFreq(freq_write, length);
        p_hc->write_entropy = (p_hc->write_entropy * p_hc->write_cnt_times + write_entropy) / (p_hc->write_cnt_times + 1);
        p_hc->write_cnt_times += 1;
        auto t = GetNtSystemTime();
        if (p_hc->first_write_timestamp == 0) {
            p_hc->first_write_timestamp = t;
        }
        p_hc->last_write_timestamp = t;

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileWrite(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreReadFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* p_completion_context)
    {
        if (data->RequestorMode == KernelMode ||
            FsRtlIsPagingFile(flt_objects->FileObject) ||
            IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (p_completion_context == nullptr) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        PHANDLE_CONTEXT p_hc = nullptr;

        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status)) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        *p_completion_context = p_hc;

        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostReadFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID p_completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        if (p_completion_context == nullptr) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        PHANDLE_CONTEXT p_hc = (PHANDLE_CONTEXT)(p_completion_context);
        defer(FltReleaseContext(p_hc););

        if (!NT_SUCCESS(data->IoStatus.Status))
            return FLT_POSTOP_FINISHED_PROCESSING;

        UCHAR* buffer = nullptr;

        const auto& read_params = data->Iopb->Parameters.Read;
        if (read_params.MdlAddress != nullptr) {
            buffer = (unsigned char*)MmGetSystemAddressForMdlSafe(read_params.MdlAddress,
                NormalPagePriority | MdlMappingNoExecute);
            if (buffer == nullptr) {
                return FLT_POSTOP_FINISHED_PROCESSING;
            }
        }
        else {
            __try {
                buffer = (UCHAR*)read_params.ReadBuffer;
                ProbeForRead(buffer, read_params.Length, 1);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                buffer = nullptr;
            }
        }
        if (buffer == nullptr) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        auto freq_read = new LONGLONG[256];
        RtlZeroMemory(freq_read, sizeof(LONGLONG) * 256);
        defer(delete[] freq_read;);

        ULONG length = min(read_params.Length, (ULONG)data->IoStatus.Information);

        if (length == 0) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        __try {
            for (ULONG i = 0; i < length; i++) {
                ++freq_read[buffer[i]];
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        p_hc->is_read = true;
        p_hc->read_cnt_bytes += length;
        auto read_entropy = math::ComputeByteEntropyFromFreq(freq_read, length);
        p_hc->read_entropy = (p_hc->read_entropy * p_hc->read_cnt_times + read_entropy) / (p_hc->read_cnt_times + 1);
        p_hc->read_cnt_times += 1;
        auto t = GetNtSystemTime();
        if (p_hc->first_read_timestamp == 0) {
            p_hc->first_read_timestamp = t;
        }
        p_hc->last_read_timestamp = t;

        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        NTSTATUS status;

        if (data->RequestorMode == KernelMode) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        //DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        PHANDLE_CONTEXT p_hc = nullptr;

        status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status))
        {
            if (status != STATUS_NOT_FOUND) {
                //DebugMessage("FltGetStreamHandleContext failed: %x", status);
            }
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        
        auto& set_info_params = data->Iopb->Parameters.SetFileInformation;

        auto file_info_class = set_info_params.FileInformationClass;

        if (file_info_class == FileRenameInformation 
            || file_info_class == FileRenameInformationBypassAccessCheck
            || file_info_class == FileRenameInformationEx 
            || file_info_class == FileRenameInformationExBypassAccessCheck)
        {
            PFILE_RENAME_INFORMATION target_info = (PFILE_RENAME_INFORMATION)data->Iopb->Parameters.SetFileInformation.InfoBuffer;
            PFLT_FILE_NAME_INFORMATION p_name_info;

            status = FltGetDestinationFileNameInformation(
                flt_objects->Instance,
                flt_objects->FileObject,
                target_info->RootDirectory,
                target_info->FileName,
                target_info->FileNameLength,
                FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                &p_name_info
            );

            if (NT_SUCCESS(status))
            {
                //DebugMessage("File: %ws, renamed to %ws", p_hc->path, name_info->Name.Buffer);

                p_hc->is_renamed = true;
                p_hc->new_path = p_name_info->Name;
                FltReleaseFileNameInformation(p_name_info);
            }
        }
        else if (file_info_class == FileDispositionInformation || file_info_class == FileDispositionInformationEx) {
            p_hc->is_deleted = true;
        }
        else if (file_info_class == FileAllocationInformation) {
            p_hc->is_alloc = true;
        }
        else if (file_info_class == FileEndOfFileInformation) {
            p_hc->is_eof = true;
        }
        else if (file_info_class == FileValidDataLengthInformation) {
            p_hc->is_fvdli = true;
        }

        FltReleaseContext(p_hc);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
		if (data->RequestorMode == KernelMode) {
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}

        if (IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        auto& acquire_params = data->Iopb->Parameters.AcquireForSectionSynchronization;

        if (acquire_params.SyncType == SyncTypeCreateSection &&
            (acquire_params.PageProtection == PAGE_READWRITE || acquire_params.PageProtection == PAGE_EXECUTE_READWRITE))
        {
            PHANDLE_CONTEXT p_hc = nullptr;

            std::WString current_path = flt::GetFileFullPathName(data);
            if (current_path.Size() == 0) {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
            if (!NT_SUCCESS(status))
            {
                status = FltAllocateContext(flt_objects->Filter, FLT_FILE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
                if (!NT_SUCCESS(status)) {
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                
                memset(p_hc, 0, sizeof(HANDLE_CONTEXT));

                p_hc->path = current_path;
                p_hc->requestor_pid = FltGetRequestorProcessId(data);
                p_hc->is_mmap_open = true;
                status = FltSetFileContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(p_hc), nullptr);
                if (!NT_SUCCESS(status)) {
                    //DebugMessage("FltSetFileContext failed: %x", status);
                }

                FltReleaseContext(p_hc);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            FltReleaseContext(p_hc);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
		//DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileSystemControl(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        if (data->RequestorMode == KernelMode ||
            FsRtlIsPagingFile(flt_objects->FileObject) ||        // not interested in writes to the paging file 
            IoGetTopLevelIrp() != NULL) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        PHANDLE_CONTEXT p_hc = nullptr;
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status)) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        defer(FltReleaseContext(p_hc););
        switch (data->Iopb->Parameters.FileSystemControl.Common.FsControlCode) {
        case FSCTL_OFFLOAD_WRITE:
            p_hc->is_fs_ow = true;
            break;
        case FSCTL_WRITE_RAW_ENCRYPTED:
            p_hc->is_fs_wre = true;
            break;
        case FSCTL_SET_ZERO_DATA:
            p_hc->is_fs_szd = true;
            break;
        default: break;
        }
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileSystemControl(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        PHANDLE_CONTEXT p_hc = nullptr;
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_hc));
        if (!NT_SUCCESS(status)) {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        //FltDeleteContext(p_hc);
        FltReleaseContext(p_hc);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        // complete the request asynchronously at passive level 
        //PFLT_DEFERRED_IO_WORKITEM EvalWorkItem = FltAllocateDeferredIoWorkItem();

        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    static void LogFileEvent(const collector::HANDLE_CONTEXT* p_hc)
    {
        mtx.Lock();
        if (sp->Find(p_hc->requestor_pid) == sp->End()) {
            mtx.Unlock();
            PushToLogQueue(L"P,%llu,%ws\n", (ull)p_hc->requestor_pid, p_hc->process_path.Data());
            mtx.Lock();
            sp->Insert(p_hc->requestor_pid);
        }
        mtx.Unlock();

        auto hf = HashWstring(p_hc->path);
        mtx.Lock();
        if (sf->Find(hf) == sf->End()) {
            mtx.Unlock();
            PushToLogQueue(L"F,%llu,%ws\n", hf, p_hc->path.Data());
            mtx.Lock();
            sf->Insert(hf);
            if (sf->Size() == 100'000) {
                sf->Clear();
            }
        }
        mtx.Unlock();

        auto hfn = HashWstring(p_hc->new_path);
        mtx.Lock();
        if (sf->Find(hfn) == sf->End()) {
            mtx.Unlock();
            PushToLogQueue(L"F,%llu,%ws\n", hfn, p_hc->new_path.Data());
            mtx.Lock();
            sf->Insert(hfn);
            if (sf->Size() == 100'000) {
                sf->Clear();
            }
        }
        mtx.Unlock();

        PushToLogQueue(
            L"O,"
            L"%lu,"        // pid
            L"%llu,"       // path hash
            L"%lld,"       // file size

            L"%d,%d,"      // is_read,is_modified

            // READ
            L"%lld,%lld,%lld,%lld,%lld,"

            // WRITE
            L"%lld,%lld,%lld,%lld,%lld,"

            // RENAME
            L"%d,%llu,"

            // CREATE / DELETE
            L"%d,%d,"

            // FLAGS
            L"%d,%d,%d,%d,%d,%d,%d,%d\n",

            p_hc->requestor_pid,
            hf,
            p_hc->file_size,

            p_hc->is_read,
            p_hc->is_modified,

            // READ
            (LONGLONG)(p_hc->read_entropy * 10000),
            p_hc->first_read_timestamp,
            p_hc->last_read_timestamp,
            p_hc->read_cnt_bytes,
            p_hc->read_cnt_times,

            // WRITE
            (LONGLONG)(p_hc->write_entropy * 10000),
            p_hc->first_write_timestamp,
            p_hc->last_write_timestamp,
            p_hc->write_cnt_bytes,
            p_hc->write_cnt_times,

            // RENAME
            p_hc->is_renamed,
            hfn,

            // CREATE / DELETE
            p_hc->is_created,
            p_hc->is_deleted,

            // FLAGS
            p_hc->is_alloc,
            p_hc->is_eof,
            p_hc->is_fvdli,
            p_hc->is_fs_ow,
            p_hc->is_fs_wre,
            p_hc->is_fs_szd,
            p_hc->is_mmap_open,
            p_hc->is_mmap_modified
        );

    }

    void ContextCleanup(PFLT_CONTEXT context, FLT_CONTEXT_TYPE context_type)
    {
        if (context_type == FLT_STREAMHANDLE_CONTEXT || context_type == FLT_FILE_CONTEXT)
        {
            collector::HANDLE_CONTEXT* p_hc = (collector::HANDLE_CONTEXT*)context;
            if (p_hc != nullptr)
            {
                int action_cnt = 
                    p_hc->is_modified +
                    p_hc->is_read +
                    p_hc->is_renamed +
                    //p_hc->is_created +
                    p_hc->is_deleted +
                    p_hc->is_alloc +
                    p_hc->is_eof +
                    p_hc->is_fvdli +
                    p_hc->is_fs_ow +
                    p_hc->is_fs_wre +
                    p_hc->is_fs_szd +
                    p_hc->is_mmap_modified;

                if (action_cnt == 0) {
                    return;
                }
                LogFileEvent(p_hc);

                if (p_hc->is_deleted == true 
                    || (action_cnt == 1 && p_hc->is_read == true)) {
                    return;
                }
                
                std::WString path;

                if (p_hc->is_renamed == true) {
                    path = p_hc->new_path;
                }
                else {
                    path = p_hc->path;
                }
                if (path.HasCiPrefix(L"\\device\\harddiskvolume3\\windows\\") == true
                    || path.HasCiPrefix(L"\\device\\harddiskvolume3\\program files\\") == true
                    || path.HasCiPrefix(L"\\device\\harddiskvolume3\\program files (x86)\\") == true
                    || path.HasCiPrefix(L"\\device\\harddiskvolume3\\users\\hieu\\appdata\\") == true
                    || path.HasCiPrefix(L"\\device\\harddiskvolume3\\programdata\\") == true
                    ) {
                    return;
                }
                //DebugMessage("Send %ws", tmp_path.Data());
                com::kComPort->Send(path.Data(), (path.Size() + 1) * sizeof(WCHAR));
            }
        }
        return;
    }
} // namespace collector
