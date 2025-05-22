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
        DebugMessage("%ws", __FUNCTIONW__);

        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreFileCreate, PostFileCreate });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CLOSE, PreFileClose, PostFileClose });
        reg::kFltFuncVector->PushBack({ IRP_MJ_WRITE, PreWriteFile, PostFileWrite });
        reg::kFltFuncVector->PushBack({ IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION, PreFileAcquireForSectionSync, PostFileAcquireForSectionSync });
        reg::kFltFuncVector->PushBack({ IRP_MJ_SET_INFORMATION, PreFileSetInformation, PostFileSetInformation });

        DebugMessage("Callbacks created.");
        return;
    }

    // Huỷ đăng ký các bộ lọc bảo vệ file
	NTSTATUS FltUnload()
    {
        DebugMessage("Begin %ws", __FUNCTIONW__);

        String<WCHAR> file_path1(file::NormalizeDevicePathStr(L"\\??\\C:\\Users\\hieu\\Documents\\ggez.txt"));
        String<WCHAR> file_path2(file::NormalizeDevicePathStr(L"\\??\\E:\\ggez.txt"));
        if (file::ZwIsFileExist(file_path1) == true || file::ZwIsFileExist(file_path2) == true)
        {
            DebugMessage("Magic files exist, so we allow the driver to unload");
            return STATUS_SUCCESS;
        }
        DebugMessage("STATUS_FLT_DO_NOT_DETACH");
        return STATUS_FLT_DO_NOT_DETACH;
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

        String<WCHAR> current_path = flt::GetFileFullPathName(data);

        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1 || current_path.Find(L"\\Device\\HarddiskVolume4") != ULL_MAX)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        //DebugMessage("File %ws, instance %p, file object %p, pid %d", current_path.Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        file::FileFlt f = file::FileFlt(current_path, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject);

        if (f.Exist() == false)
        {
            //DebugMessage("File %ws not exist", current_path.Data());
            return FLT_PREOP_SUCCESS_WITH_CALLBACK;
        }

        const auto& create_params = data->Iopb->Parameters.Create;
        ULONG options = create_params.Options;
		UINT8 create_disposition = options >> 24;

        //DebugMessage("File %ws, create disposition %d", current_path.Data(), create_disposition);

        if (create_disposition == FILE_OVERWRITE_IF)
        {
            // Create a backup
            ull backup_name_size = 0;
            WCHAR* p_backup_name = new WCHAR[HIEUNT_MAX_PATH];
            if (backup::BackupFile(current_path.Data(), p_backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, nullptr))
            {
                DebugMessage("Backed up file %ws to %ws", current_path.Data(), p_backup_name);
            }
            else
            {
                DebugMessage("Backup file %ws failed", current_path.Data());
            }
            delete[] p_backup_name;
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

        String<WCHAR> current_path = flt::GetFileFullPathName(data);

        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
        {
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

		//DebugMessage("File %ws, instance %p, file object %p, pid %d", current_path.Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        const auto& create_params = data->Iopb->Parameters.Create;

        bool is_delete_on_close = FlagOn(create_params.Options, FILE_DELETE_ON_CLOSE);
        bool has_delete_access = create_params.SecurityContext->DesiredAccess & DELETE;
        bool has_write_access = create_params.SecurityContext->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA);

        if (!has_write_access && !is_delete_on_close && !has_delete_access)
        {
            //DebugMessage("File: %ws, no write access, no delete on close, no delete accesss", current_path.Data());
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        ULONG options = create_params.Options;
        ULONG create_disposition = options & 0x000000ff;

        bool is_created = (data->IoStatus.Information == FILE_CREATED);
        // Not interested in new files without write access
        if (is_created && !has_write_access)
        {
            //DebugMessage("File: %ws, created without write access", current_path.Data());
            return FLT_POSTOP_FINISHED_PROCESSING;
        }

        PHANDLE_CONTEXT handle_context = nullptr;
        NTSTATUS status = FltAllocateContext(flt_objects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
        if (!NT_SUCCESS(status))
        {
            //DebugMessage("FltAllocateContext failed: 0x%x", status);
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
            //DebugMessage("FltSetStreamHandleContext failed: %x", status);
            FltReleaseContext(handle_context);
            return FLT_POSTOP_FINISHED_PROCESSING;
        }
        else
        {
            //DebugMessage("FltSetStreamHandleContext success: handle context set %p", handle_context);
            //DebugMessage("Context info: requestor pid: %d, is_modified: %hu, is_deleted: %hu, is_created: %hu, is_renamed: %hu", handle_context->requestor_pid, handle_context->is_modified, handle_context->is_deleted, handle_context->is_created, handle_context->is_renamed);
        }

        if (handle_context->is_deleted)
        {
            // We don't need to backup if the file was just created.
            if (handle_context->is_created)
            {
                //DebugMessage("File: %ws, created", current_path.Data());
                FltReleaseContext(handle_context);
                return FLT_POSTOP_FINISHED_PROCESSING;
            }

            // Only backup if the file extension is monitored.
            if (!detector::IsExtensionMonitored(handle_context->current_path))
            {
                //DebugMessage("File: %ws, extension not monitored", handle_context->current_path);
                FltReleaseContext(handle_context);
                return FLT_POSTOP_FINISHED_PROCESSING;
            }

            //DebugMessage("File: %ws, delete on close", current_path.Data());
            // Create a backup
            ull backup_name_size = 0;
            if (backup::BackupFile(handle_context->current_path, handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
            {
                DebugMessage("Backed up file %ws to %ws", handle_context->current_path, handle_context->backup_name);
            }
            else
            {
                RtlZeroMemory(handle_context->backup_name, sizeof(handle_context->backup_name));
                DebugMessage("Backup file %ws failed", handle_context->current_path);
            }
        }

        FltReleaseContext(handle_context);
        
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    FLT_PREOP_CALLBACK_STATUS PreWriteFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        // not interested in writes to the paging file 
        if (FsRtlIsPagingFile(flt_objects->FileObject))
            return FLT_PREOP_SUCCESS_NO_CALLBACK;

        // We only care about write operation to some first bytes or some last bytes of the file.
        auto& write_params = data->Iopb->Parameters.Write;
        ull write_offset = write_params.ByteOffset.QuadPart;
        ull write_length = write_params.Length;
        // We only care about write operation to some first bytes of the file.
        if (write_length == 0)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        String<WCHAR> current_path = flt::GetFileFullPathName(data);
        if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
        {
            ////DebugMessage("File: %ws, size %llu is an error", current_path.Data(), current_path.Size());
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        file::FileFlt f = file::FileFlt(current_path, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject);

        if (!NT_SUCCESS(f.Open()))
        {
            //DebugMessage("File: %ws, open failed", f.GetPath().Data());
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        ull file_size = f.Size();
        if (write_offset != 0)
        {
            if (file_size != 0 && file_size != ULL_MAX) {
                // If file_size is archived, we only care about write operation to some first bytes and some last bytes of the file.
                if (write_offset > BEGIN_WIDTH && write_offset + write_length < file_size - END_WIDTH)
                {
                    //DebugMessage("Skip IO, file %ws, write offset: %I64d, length: %I64d, file size: %I64d", current_path.Data(), write_offset, write_length, file_size);
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
            }
        }

        PHANDLE_CONTEXT p_handle_context = nullptr;

        // If noncached paging I/O and not to the pagefile
        if (FlagOn(data->Iopb->IrpFlags, IRP_NOCACHE) && FlagOn(data->Iopb->IrpFlags, IRP_PAGING_IO))
        {
            // We do not ignore kernel mode writes here because this is where memory-mapped writes occur.
            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
				if (status != STATUS_NOT_FOUND)
				{
					//DebugMessage("FltGetFileContext failed: %x", status);
				}
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
        }
        else
        {
            if (data->RequestorMode == KernelMode)
            {
                ////DebugMessage("File: %ws, kernel mode write", current_path.Data());
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            NTSTATUS status = FltGetStreamHandleContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&p_handle_context));
            if (!NT_SUCCESS(status))
            {
				if (status != STATUS_NOT_FOUND) {
                    //DebugMessage("FltGetStreamHandleContext failed: %x", status);
                }
				return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
        }

        DebugMessage("File %ws, instance %p, file object %p, pid %d", current_path.Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        p_handle_context->is_modified = true;

        ull backup_name_size = 0;

        // We don't need to backup if the file was just created.
        if (p_handle_context->is_created == true)
        {
            //DebugMessage("File: %ws, was just created so no backup", current_path.Data());
            goto pre_write_release_context;
        }

        /*
        // Only backup if the file extension is monitored.
        if (!detector::IsExtensionMonitored(p_handle_context->current_path))
        {
            //DebugMessage("File: %ws, extension not monitored", p_handle_context->current_path);
            goto pre_write_release_context;
        }
        */
        if (file_size != 0 && file_size != ULL_MAX)
        {
            // Create a backup
            if (backup::BackupFile(p_handle_context->current_path, p_handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
            {
                DebugMessage("Backed up file %ws to %ws", p_handle_context->current_path, p_handle_context->backup_name);
            }
            else
            {
                RtlZeroMemory(p_handle_context->backup_name, sizeof(p_handle_context->backup_name));
                DebugMessage("Backup file %ws failed", p_handle_context->current_path);
            }
        }
    pre_write_release_context:
        if (data->RequestorMode == KernelMode)
        {
            FltDeleteContext(p_handle_context);
            FltReleaseContext(p_handle_context);
            ////DebugMessage("FltDeleteFileContext: %p", p_handle_context);
        }
        else
        {
            ////DebugMessage("FltReleaseContext success: %p", p_handle_context);
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
                //DebugMessage("File: %ws, renamed to %ws", p_handle_context->current_path, p_handle_context->new_path);
                FltReleaseFileNameInformation(name_info);
            }
            else
            {
                //DebugMessage("FltGetDestinationFileNameInformation failed: %x", status);
            }
            is_sensitive_info_class = false;
        }
        else if (file_info_class == FileDispositionInformation || file_info_class == FileDispositionInformationEx)
        {
            //DebugMessage("FileDispositionInformation %ws", p_handle_context->current_path);
            p_handle_context->is_deleted = true;
            is_sensitive_info_class = true;
        }
        else if (file_info_class == FileAllocationInformation)
        {
            //DebugMessage("FileAllocationInformation %ws", p_handle_context->current_path);
            is_sensitive_info_class = true;
        }
        else if (file_info_class == FileEndOfFileInformation)
        {
            //DebugMessage("FileAllocationInformation %ws", p_handle_context->current_path);
            is_sensitive_info_class = true;
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
                //DebugMessage("File: %ws was just created, so no backup.", p_handle_context->current_path);
                FltReleaseContext(p_handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            /*
            // Only backup if the file extension is monitored.
            if (!detector::IsExtensionMonitored(p_handle_context->current_path))
            {
                //DebugMessage("File: %ws, extension not monitored", p_handle_context->current_path);
                FltReleaseContext(p_handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            // We don't need to backup if the file was just renamed and kept the original extension.
            if (p_handle_context->new_path[0] != L'\0' 
                && (file_info_class == FileDispositionInformation || file_info_class == FileDispositionInformationEx) 
                && detector::IsSameFileExtension(p_handle_context->current_path, p_handle_context->new_path))
            {
                //DebugMessage("File %ws was renamed to %ws, but kept the same extension", p_handle_context->current_path, p_handle_context->new_path);
                FltReleaseContext(p_handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            */

            // Create a backup
            ull backup_name_size = 0;
            if (backup::BackupFile(p_handle_context->current_path, p_handle_context->backup_name, HIEUNT_MAX_PATH, &backup_name_size, flt_objects->Filter, flt_objects->Instance, flt_objects->FileObject))
            {
                //DebugMessage("Backed up file %ws to %ws", p_handle_context->current_path, p_handle_context->backup_name);
            }
            else
            {
                RtlZeroMemory(p_handle_context->backup_name, sizeof(p_handle_context->backup_name));
                //DebugMessage("Backup file %ws failed", p_handle_context->current_path);
            }
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

		////DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

        auto& acquire_params = data->Iopb->Parameters.AcquireForSectionSynchronization;

        if (acquire_params.SyncType == SyncTypeCreateSection &&
            (acquire_params.PageProtection == PAGE_READWRITE || acquire_params.PageProtection == PAGE_EXECUTE_READWRITE))
        {
			//DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));
            //DebugMessage("AcquireForSectionSync: SyncTypeCreateSection, PageProtection: %x, file %ws", acquire_params.PageProtection, flt::GetFileFullPathName(data).Data());

            PHANDLE_CONTEXT handle_context = nullptr;

            String<WCHAR> current_path = flt::GetFileFullPathName(data);
            if (current_path.Size() == 0 || current_path.Size() > HIEUNT_MAX_PATH - 1)
            {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            NTSTATUS status = FltGetFileContext(flt_objects->Instance, flt_objects->FileObject, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
            if (!NT_SUCCESS(status))
            {
                //DebugMessage("FltGetFileContext failed 0x%x, alloc once", status);
                status = FltAllocateContext(flt_objects->Filter, FLT_FILE_CONTEXT, sizeof(HANDLE_CONTEXT), NonPagedPool, reinterpret_cast<PFLT_CONTEXT*>(&handle_context));
                if (!NT_SUCCESS(status))
                {
                    //DebugMessage("FltAllocateContext failed: %x", status);
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                
                memset(handle_context, 0, sizeof(HANDLE_CONTEXT));

                RtlCopyMemory(handle_context->current_path, current_path.Data(), current_path.Size() * sizeof(WCHAR));
                handle_context->requestor_pid = FltGetRequestorProcessId(data);
                
                status = FltSetFileContext(flt_objects->Instance, flt_objects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, reinterpret_cast<PFLT_CONTEXT>(handle_context), nullptr);
                if (!NT_SUCCESS(status))
                {
                    //DebugMessage("FltSetFileContext failed: %x", status);
                }

                //DebugMessage("AcquireForSectionSync: file %ws, context 0x%p, requestor pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d", handle_context->current_path, handle_context, handle_context->requestor_pid, handle_context->is_modified, handle_context->is_deleted, handle_context->is_created, handle_context->is_renamed);

                FltReleaseContext(handle_context);
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            else
            {
                //DebugMessage("FltGetFileContext success, skip this I/O");
            }
            FltReleaseContext(handle_context);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    FLT_POSTOP_CALLBACK_STATUS PostFileAcquireForSectionSync(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID completion_context, FLT_POST_OPERATION_FLAGS flags)
    {
		////DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

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
            ////DebugMessage("FltGetStreamHandleContext success: handle context %p", p_handle_context);
        }
		//DebugMessage("File %ws, instance %p, file object %p, pid %d", flt::GetFileFullPathName(data).Data(), flt_objects->Instance, flt_objects->FileObject, FltGetRequestorProcessId(data));

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
