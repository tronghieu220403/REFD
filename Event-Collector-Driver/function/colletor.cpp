#include "collector.h"
#include "../std/file/file.h"   

namespace collector
{

    // Đăng ký các callback bảo vệ process và thread
    void DrvRegister()
    {
        
    }

    // Huỷ đăng ký các callback bảo vệ process và thread
    void DrvUnload()
    {

    }

    // Đăng ký các bộ lọc bảo vệ file
    void FltRegister()
    {
        //DebugMessage("%ws", __FUNCTIONW__);

        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreFileCreate, PostFileCreate });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CLOSE, PreFileClose, PostFileClose });
        reg::kFltFuncVector->PushBack({ IRP_MJ_WRITE, PreWriteFile, PostFileWrite });
        reg::kFltFuncVector->PushBack({ IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION, PreFileAcquireForSectionSync, PostFileAcquireForSectionSync });
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
        if (data->RequestorMode == KernelMode)
        {
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

        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1 || current_path.HasCiPrefix(L"\\Device\\HarddiskVolume4\\hieunt210330") == true)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        UNREFERENCED_PARAMETER(completion_context);

        if (!NT_SUCCESS(data->IoStatus.Status))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (flags & FLTFL_POST_OPERATION_DRAINING)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (!FltSupportsStreamHandleContexts(flt_objects->FileObject))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        std::WString current_path = flt::GetFileFullPathName(data);

        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        const auto& create_params = data->Iopb->Parameters.Create;

        bool is_delete_on_close = FlagOn(create_params.Options, FILE_DELETE_ON_CLOSE);
        bool has_write_access = create_params.SecurityContext->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA);
        bool has_delete_access = create_params.SecurityContext->DesiredAccess & DELETE;

        bool is_created = (data->IoStatus.Information == FILE_CREATED);

        // Not interested in files without write and delete access
        if (!is_created && !has_write_access && !is_delete_on_close && !has_delete_access)
        {
            /*
            if (current_path.FindFirstOf(L"hieunt-0075") != ULL_MAX)
            {
                DebugMessage("File: %ws, Not interested in files without create, write and delete access", current_path.Data());
            }
            */
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        ULONG options = create_params.Options;

        PHANDLE_CONTEXT p_handle_context = nullptr;
        NTSTATUS status = FltAllocateContext(flt_objects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
        if (!NT_SUCCESS(status))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        memset(p_handle_context, 0, sizeof(HANDLE_CONTEXT));
        p_handle_context->requestor_pid = FltGetRequestorProcessId(data);
        p_handle_context->is_created = is_created;
        p_handle_context->is_deleted = is_delete_on_close;
        RtlCopyMemory(p_handle_context->path, current_path.Data(), current_path.Size() * sizeof(WCHAR));
        
        status = FltSetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(p_handle_context), nullptr);

        FltReleaseContext(p_handle_context);
        
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        // not interested in writes to the paging file 
        if (FsRtlIsPagingFile(flt_objects->FileObject))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        std::WString current_path = flt::GetFileFullPathName(data);
        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
        {
            //DebugMessage("File: %ws, size %llu is an error", current_path.Data(), current_path.Size());
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        PHANDLE_CONTEXT p_handle_context = nullptr;

        bool is_file_context = false;

        if (FlagOn(data->Iopb->IrpFlags, IRP_NOCACHE) && FlagOn(data->Iopb->IrpFlags, IRP_PAGING_IO))
        // If noncached paging I/O and not to the pagefile
        {
            // We do not ignore kernel mode writes here because this is where memory-mapped writes occur.
            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            FltDeleteContext(p_handle_context);
        }
        else
        {
            if (data->RequestorMode == KernelMode)
            {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
				return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            p_handle_context->is_modified = true;
        }

        FltReleaseContext(p_handle_context);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileWrite(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        NTSTATUS status;

        if (data->RequestorMode == KernelMode)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        //DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        PHANDLE_CONTEXT p_handle_context = nullptr;

        status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
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
            PFLT_FILE_NAME_INFORMATION name_info;

            status = FltGetDestinationFileNameInformation(
                flt_objects->Instance,
                flt_objects->FileObject,
                target_info->RootDirectory,
                target_info->FileName,
                target_info->FileNameLength,
                FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                &name_info
            );

            if (NT_SUCCESS(status))
            {
                //DebugMessage("File: %ws, renamed to %ws", p_handle_context->path, name_info->Name.Buffer);

                p_handle_context->is_renamed = true;
                if (name_info->Name.Length > 0 && name_info->Name.Length < (HIEUNT_MAX_PATH - 1) * sizeof(WCHAR))
                {
                    RtlCopyMemory(p_handle_context->path, name_info->Name.Buffer, name_info->Name.Length);
                    p_handle_context->path[name_info->Name.Length / sizeof(WCHAR)] = L'\0';
                }
                else
                {
                    RtlZeroMemory(p_handle_context->path, sizeof(p_handle_context->path));
                }
                FltReleaseFileNameInformation(name_info);
            }
        }
        else if (file_info_class == FileDispositionInformation || file_info_class == FileDispositionInformationEx)
        {
            p_handle_context->is_deleted = true;
        }
        else if (file_info_class == FileAllocationInformation)
        {

        }
        else if (file_info_class == FileEndOfFileInformation)
        {

        }


        FltReleaseContext(p_handle_context);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        /*
        //DebugMessage("%ws, instance %p, file object %p, pid %d", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));
        if (!NT_SUCCESS(data->IoStatus.Status))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (flags & FLTFL_POST_OPERATION_DRAINING)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (data->RequestorMode == KernelMode)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        */

        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
		if (data->RequestorMode == KernelMode)
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}

        auto& acquire_params = data->Iopb->Parameters.AcquireForSectionSynchronization;

        if (acquire_params.SyncType == SyncTypeCreateSection &&
            (acquire_params.PageProtection == PAGE_READWRITE || acquire_params.PageProtection == PAGE_EXECUTE_READWRITE))
        {
            PHANDLE_CONTEXT handle_context = nullptr;

            std::WString current_path = flt::GetFileFullPathName(data);
            if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
            {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
            if (!NT_SUCCESS(status))
            {
                status = FltAllocateContext(flt_objects->Filter, FLT_FILE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
                if (!NT_SUCCESS(status))
                {
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                
                memset(handle_context, 0, sizeof(HANDLE_CONTEXT));

                RtlCopyMemory(handle_context->path, current_path.Data(), current_path.Size() * sizeof(WCHAR));
                handle_context->requestor_pid = FltGetRequestorProcessId(data);
                handle_context->is_modified = true;
                status = FltSetFileContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(handle_context), nullptr);
                if (!NT_SUCCESS(status))
                {
                    //DebugMessage("FltSetFileContext failed: %x", status);
                }

                FltReleaseContext(handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            FltReleaseContext(handle_context);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
		//DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        if (!NT_SUCCESS(data->IoStatus.Status))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (flags & FLTFL_POST_OPERATION_DRAINING)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        PHANDLE_CONTEXT p_handle_context = nullptr;
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
        if (!NT_SUCCESS(status))
        {
            //if (status != STATUS_NOT_FOUND) { //DebugMessage("FltGetStreamHandleContext failed: %x", status); }
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        else
        {
            //DebugMessage("FltGetStreamHandleContext success: handle context %p", p_handle_context);
        }

        //DebugMessage("File: %ws, requestor pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d", p_handle_context->current_path, p_handle_context->requestor_pid, p_handle_context->is_modified, p_handle_context->is_deleted, p_handle_context->is_created, p_handle_context->is_renamed);
        FltReleaseContext(p_handle_context);

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        // complete the request asynchronously at passive level 
        //PFLT_DEFERRED_IO_WORKITEM EvalWorkItem = FltAllocateDeferredIoWorkItem();

        return FLT_POSTOP_FINISHED_PROCESSING;
    }
} // namespace collector
