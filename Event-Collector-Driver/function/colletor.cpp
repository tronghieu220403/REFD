#include "collector.h"
#include "../std/file/file.h"   
#include "backup.h"

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
        //DebugMessage("%ws\n", __FUNCTIONW__);

        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreDoNothing, PostFileCreate });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CLOSE, PreFileClose, PostFileClose });
        reg::kFltFuncVector->PushBack({ IRP_MJ_WRITE, PreWriteFile, PostFileWrite });
        reg::kFltFuncVector->PushBack({ IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION, PreFileAcquireForSectionSync, PostFileAcquireForSectionSync });
        reg::kFltFuncVector->PushBack({ IRP_MJ_SET_INFORMATION, PreFileSetInformation, PostFileSetInformation });

        //DebugMessage("FltRegister callback created.");
        return;
    }

    // Huỷ đăng ký các bộ lọc bảo vệ file
    void FltUnload()
    {
        //DebugMessage("%ws\n", __FUNCTIONW__);
    }

    FLT_PREOP_CALLBACK_STATUS PreDoNothing(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {

        UNREFERENCED_PARAMETER(completion_context);

        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        if (!NT_SUCCESS(data->IoStatus.Status))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (flags & FLTFL_POST_OPERATION_DRAINING)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        //  Directory opens don't need to be scanned.
        if (FlagOn(data->Iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE))
            return FLT_POSTOP_FINISHED_PROCESSING;

        //  Skip pre-rename operations which always open a directory.
        if (FlagOn(data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY))
            return FLT_POSTOP_FINISHED_PROCESSING;

        //  Skip paging files.
        if (FlagOn(data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE))
            return FLT_POSTOP_FINISHED_PROCESSING;

        //  Skip scanning DASD opens 
        if (FlagOn(flt_objects->FileObject->Flags, FO_VOLUME_OPEN))
            return FLT_POSTOP_FINISHED_PROCESSING;

        // Skip kernel mode
        if (data->RequestorMode == KernelMode)
        {
            //DebugMessage("File: %ws, kernel mode\n", flt_objects->FileObject->FileName.Buffer);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (!FltSupportsStreamHandleContexts(flt_objects->FileObject))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        String<WCHAR> current_path = flt::GetFileFullPathName(data);

        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        //DebugMessage("File: %ws\n", current_path.Data());

        ULONG_PTR stack_low;
        ULONG_PTR stack_high;
        PFILE_OBJECT file_obj = data->Iopb->TargetFileObject;

        IoGetStackLimits(&stack_low, &stack_high);
        if (((ULONG_PTR)file_obj > stack_low) &&
            ((ULONG_PTR)file_obj < stack_high))
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        const auto& create_params = data->Iopb->Parameters.Create;

        bool is_delete_on_close = FlagOn(create_params.Options, FILE_DELETE_ON_CLOSE);
        bool has_delete_access = create_params.SecurityContext->DesiredAccess & DELETE;
        bool has_write_access = create_params.SecurityContext->DesiredAccess & FILE_WRITE_DATA;

        if (!has_write_access && !is_delete_on_close && !has_delete_access)
        {
            //DebugMessage("File: %ws, no write access\n", current_path.Data());
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        ULONG options = create_params.Options;
        ULONG create_disposition = options & 0x000000ff;

        bool is_created = (data->IoStatus.Information == FILE_CREATED);
        // Not interested in new files without write access
        if (is_created && !has_write_access)
        {
            //DebugMessage("File: %ws, created without write access\n", current_path.Data());
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        PHANDLE_CONTEXT handle_context = nullptr;
        NTSTATUS status = FltAllocateContext(flt_objects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltAllocateContext failed: 0x%x\n", status);
            STATUS_FLT_CONTEXT_ALLOCATION_NOT_FOUND;
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        memset(handle_context, 0, sizeof(HANDLE_CONTEXT));
        handle_context->requestor_pid = FltGetRequestorProcessId(data);
        handle_context->is_created = is_created;
        handle_context->is_deleted = is_delete_on_close;
        RtlCopyMemory(handle_context->current_path, current_path.Data(), current_path.Size() * sizeof(WCHAR));
        
        status = FltSetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(handle_context), nullptr);

        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltSetStreamHandleContext failed: %x\n", status);
            FltReleaseContext(handle_context);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        else
        {
            //DebugMessage("FltSetStreamHandleContext success: handle context set %p\n", handle_context);
            //DebugMessage("Context info: requestor pid: %d, is_modified: %hu, is_deleted: %hu, is_created: %hu, is_renamed: %hu\n", handle_context->requestor_pid, handle_context->is_modified, handle_context->is_deleted, handle_context->is_created, handle_context->is_renamed);
        }

        if (handle_context->is_deleted)
        {
            // We don't need to backup if the file was just created.
            if (handle_context->is_created)
            {
                //DebugMessage("File: %ws, created\n", current_path.Data());
                FltReleaseContext(handle_context);
                return FLT_POSTOP_FINISHED_PROCESSING;
            }

            // Only backup if the file extension is monitored.
            if (!detector::IsExtensionMonitored(handle_context->current_path))
            {
                //DebugMessage("File: %ws, extension not monitored\n", handle_context->current_path);
                FltReleaseContext(handle_context);
                return FLT_POSTOP_FINISHED_PROCESSING;
            }

            //DebugMessage("File: %ws, delete on close\n", current_path.Data());
            // Create a backup
            ull backup_name_size = 0;
            if (backup::BackupFile(handle_context->current_path, handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
            {
                //DebugMessage("Backup file %ws to %ws\n", handle_context->current_path, handle_context->backup_name);
            }
            else
            {
                RtlZeroMemory(handle_context->backup_name, sizeof(handle_context->backup_name));
                //DebugMessage("Backup file %ws failed\n", handle_context->current_path);
            }
        }

        FltReleaseContext(handle_context);
        
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));
        
        // not interested in writes to the paging file 
        if (FsRtlIsPagingFile(flt_objects->FileObject))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        String<WCHAR> current_path = flt::GetFileFullPathName(data);
        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH)
        {
            //DebugMessage("File: %ws, size error\n", current_path.Data());
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        //DebugMessage("File: %ws\n", current_path.Data());

        file::FileFlt f = file::FileFlt(current_path, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject);
        if (f.Open() == false)
        {
            //DebugMessage("File: %ws, open failed\n", f.GetPath().Data());
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        ull file_size = f.Size();

        // We only care about write operation to some first bytes or some last bytes of the file.
        auto& write_params = data->Iopb->Parameters.Write;
        ull write_offset = write_params.ByteOffset.QuadPart;
        ull write_length = write_params.Length;
        if (write_length == 0 || (write_offset > BEGIN_WIDTH && write_offset + write_length < file_size - END_WIDTH))
        {
            //DebugMessage("Skip IO, file %ws, write offset: %I64d, length: %I64d, file size: %I64d\n", current_path.Data(), write_offset, write_length, file_size);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        else
        {
            //DebugMessage("File: %ws, write offset: %I64d, length: %I64d, file size: %I64d\n", current_path.Data(), write_offset, write_length, file_size);
        }
        PHANDLE_CONTEXT p_handle_context = nullptr;

        // If noncached paging I/O and not to the pagefile
        if (FlagOn(data->Iopb->IrpFlags, IRP_NOCACHE) && FlagOn(data->Iopb->IrpFlags, IRP_PAGING_IO))
        {
            //DebugMessage("File: %ws, noncached paging I/O\n", current_path.Data());
            // We do not ignore kernel mode writes here because this is where memory-mapped writes occur.
            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
                //DebugMessage("FltGetFileContext failed: %x\n", status);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
        }
        else
        {
            if (data->RequestorMode == KernelMode)
            {
                //DebugMessage("File: %ws, kernel mode write\n", current_path.Data());
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
                //DebugMessage("FltGetStreamHandleContext failed: %x\n", status);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
        }

        p_handle_context->is_modified = true;

        ull backup_name_size = 0;

        // We don't need to backup if the file was just created.
        if (p_handle_context->is_created)
        {
            //DebugMessage("File: %ws, was just created so no backup\n", current_path.Data());
            goto pre_write_release_context;
        }

        // Only backup if the file extension is monitored.
        if (!detector::IsExtensionMonitored(p_handle_context->current_path))
        {
            //DebugMessage("File: %ws, extension not monitored\n", p_handle_context->current_path);
            goto pre_write_release_context;
        }

        // Create a backup
        if (backup::BackupFile(p_handle_context->current_path, p_handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
        {
            //DebugMessage("Backup file %ws to %ws\n", p_handle_context->current_path, p_handle_context->backup_name);
        }
        else
        {
            RtlZeroMemory(p_handle_context->backup_name, sizeof(p_handle_context->backup_name));
            //DebugMessage("Backup file %ws failed\n", p_handle_context->current_path);
        }
    pre_write_release_context:
        if (data->RequestorMode == KernelMode)
        {
            FltDeleteContext(p_handle_context);
            FltReleaseContext(p_handle_context);
            //DebugMessage("FltDeleteFileContext: %p\n", p_handle_context);
        }
        else
        {
            //DebugMessage("FltReleaseContext success: %p\n", p_handle_context);
            FltReleaseContext(p_handle_context);
        }
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileWrite(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));
        NTSTATUS status;

        if (data->RequestorMode == KernelMode)
        {
            //DebugMessage("File: %ws, kernel mode\n", flt_objects->FileObject->FileName.Buffer);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        
        PHANDLE_CONTEXT p_handle_context = nullptr;

        status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltGetStreamHandleContext failed: %x\n", status);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        
        auto& set_info_params = data->Iopb->Parameters.SetFileInformation;

        auto file_info_class = set_info_params.FileInformationClass;

        bool is_sensitive_info_class = true;

        if (file_info_class == FileRenameInformation || file_info_class == FileRenameInformationEx)
        {
            //DebugMessage("FileRenameInformation");

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
                p_handle_context->is_renamed = true;
                if (name_info->Name.Length > 0 && name_info->Name.Length < (HIEUNT_MAX_PATH - 1) * sizeof(WCHAR))
                {
                    RtlCopyMemory(p_handle_context->new_path, name_info->Name.Buffer, name_info->Name.Length);
                    p_handle_context->new_path[name_info->Name.Length / sizeof(WCHAR)] = L'\0';
                }
                else
                {
                    RtlZeroMemory(p_handle_context->new_path, sizeof(p_handle_context->new_path));
                }
                //DebugMessage("File: %ws, renamed to %ws\n", p_handle_context->current_path, p_handle_context->new_path);
                FltReleaseFileNameInformation(name_info);
            }
            else
            {
                //DebugMessage("FltGetDestinationFileNameInformation failed: %x\n", status);
            }
        }
        else if (file_info_class == FileDispositionInformation || file_info_class == FileDispositionInformationEx)
        {
            //DebugMessage("FileDispositionInformation %ws", flt_objects->FileObject->FileName.Buffer);
            p_handle_context->is_deleted = true;
        }
        else
        {
            is_sensitive_info_class = false;
        }
        
        if (is_sensitive_info_class == true)
        {
            // We don't need to backup if the file was just created.
            if (p_handle_context->is_created)
            {
                //DebugMessage("File: %ws was just created, so no backup.\n", p_handle_context->current_path);
                FltReleaseContext(p_handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            // Only backup if the file extension is monitored.
            if (!detector::IsExtensionMonitored(p_handle_context->current_path))
            {
                //DebugMessage("File: %ws, extension not monitored\n", p_handle_context->current_path);
                FltReleaseContext(p_handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            if (p_handle_context->new_path[0] != L'\0')
            {
                // We don't need to backup if the file was just renamed and kept the original extension.
                if (detector::IsSameFileExtension(p_handle_context->current_path, p_handle_context->new_path))
                {
                    //DebugMessage("File %ws was renamed to %ws, but kept the same extension\n", p_handle_context->current_path, p_handle_context->new_path);
                    FltReleaseContext(p_handle_context);
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
            }
            
            // Create a backup
            ull backup_name_size = 0;
            if (backup::BackupFile(p_handle_context->current_path, p_handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
            {
                //DebugMessage("Backup file %ws to %ws\n", p_handle_context->current_path, p_handle_context->backup_name);
            }
            else
            {
                RtlZeroMemory(p_handle_context->backup_name, sizeof(p_handle_context->backup_name));
                //DebugMessage("Backup file %ws failed\n", p_handle_context->current_path);
            }
        }
        FltReleaseContext(p_handle_context);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileSetInformation(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        /*
        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));
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
        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        auto& acquire_params = data->Iopb->Parameters.AcquireForSectionSynchronization;

        if (acquire_params.SyncType == SyncTypeCreateSection &&
            (acquire_params.PageProtection == PAGE_READWRITE || acquire_params.PageProtection == PAGE_EXECUTE_READWRITE))
        {
            //DebugMessage("AcquireForSectionSync: SyncTypeCreateSection, PageProtection: %x\n", acquire_params.PageProtection);

            PHANDLE_CONTEXT handle_context = nullptr;

            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
            if (!NT_SUCCESS(status))
            {
                //DebugMessage("FltGetFileContext failed 0x%x, alloc once\n", status);
                status = FltAllocateContext(flt_objects->Filter, FLT_FILE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
                if (!NT_SUCCESS(status))
                {
                    //DebugMessage("FltAllocateContext failed: %x\n", status);
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                
                memset(handle_context, 0, sizeof(HANDLE_CONTEXT));

                String<WCHAR> current_path = flt::GetFileFullPathName(data);
                if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH)
                {
                    FltReleaseContext(handle_context);
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                RtlCopyMemory(handle_context->current_path, current_path.Data(), current_path.Size() * sizeof(WCHAR));
                handle_context->requestor_pid = FltGetRequestorProcessId(data);
                
                status = FltSetFileContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(handle_context), nullptr);
                if (!NT_SUCCESS(status))
                {
                    //DebugMessage("FltSetFileContext failed: %x\n", status);
                }

                //DebugMessage("AcquireForSectionSync: file %ws, context 0x%p, requestor pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d\n", handle_context->current_path, handle_context, handle_context->requestor_pid, handle_context->is_modified, handle_context->is_deleted, handle_context->is_created, handle_context->is_renamed);

                FltReleaseContext(handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            else
            {
                //DebugMessage("FltGetFileContext success, skip this I/O\n");
            }
            FltReleaseContext(handle_context);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        ////DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

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
        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        PHANDLE_CONTEXT p_handle_context = nullptr;
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltGetStreamHandleContext failed: %x\n", status);
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        else
        {
            //DebugMessage("FltGetStreamHandleContext success: handle context %p\n", p_handle_context);
        }
        //DebugMessage("File: %ws, requestor pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d\n", p_handle_context->current_path, p_handle_context->requestor_pid, p_handle_context->is_modified, p_handle_context->is_deleted, p_handle_context->is_created, p_handle_context->is_renamed);
        FltReleaseContext(p_handle_context);

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileClose(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            //DebugMessage("PostFileClose: IRQL != PASSIVE_LEVEL\n");
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        //DebugMessage("%ws, instance %p, file object %p, pid %d\n", __FUNCTIONW__, flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        PHANDLE_CONTEXT handle_context = nullptr;
        NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltGetStreamHandleContext failed: %x\n", status);
            STATUS_NOT_SUPPORTED;
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        //DebugMessage("File: %ws, requestor pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d\n", handle_context->current_path, handle_context->requestor_pid, handle_context->is_modified, handle_context->is_deleted, handle_context->is_created, handle_context->is_renamed);

        if (!NT_SUCCESS(data->IoStatus.Status))
        {
            FltReleaseContext(handle_context);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        if (flags & FLTFL_POST_OPERATION_DRAINING)
        {
            FltReleaseContext(handle_context);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        // complete the request asynchronously at passive level 
        //PFLT_DEFERRED_IO_WORKITEM EvalWorkItem = FltAllocateDeferredIoWorkItem();

        return FLT_POSTOP_FINISHED_PROCESSING;
    }
} // namespace collector
