#include "self_defense.h"

namespace self_defense {

    struct ProcessInfo
    {
        HANDLE pid = 0;
        String<WCHAR> process_path;
        bool is_protected = false;
        LARGE_INTEGER start_time = { 0 };
    };

    // Map PID với process full path, trạng thái bảo vệ, thời điểm bắt đầu của process
    static Map<HANDLE, ProcessInfo>* kProcessMap;
    static Mutex kProcessMapMutex;

    static Vector<String<WCHAR>>* kProtectedDirList;
    static Mutex kDirMutex;
    static PVOID kHandleRegistration;
    static bool kEnableProtectFile;

    // Đăng ký các callback bảo vệ process và thread
    void DrvRegister()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        kDirMutex.Create();
        kProcessMapMutex.Create();

        kProtectedDirList = new Vector<String<WCHAR>>();
        kProtectedDirList->PushBack(GetDefaultProtectedDir());

        kProcessMap = new Map<HANDLE, ProcessInfo>(); // thay đổi kiểu dữ liệu của map
        NTSTATUS status;

        // Register process creation and termination callback
        status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, FALSE);
        if (!NT_SUCCESS(status))
        {
            DebugMessage("Fail to register process notify callback: %x", status);
        }
        else
        {
            DebugMessage("Process notify callback registered");
        }

        OB_CALLBACK_REGISTRATION ob_registration = { 0 };
        OB_OPERATION_REGISTRATION op_registration[2] = {};

        ob_registration.Version = ObGetFilterVersion(); // Lấy phiên bản
        ob_registration.OperationRegistrationCount = 2;
        RtlInitUnicodeString(&ob_registration.Altitude, L"2204");
        ob_registration.RegistrationContext = NULL;

        op_registration[0].ObjectType = PsProcessType;
        op_registration[0].Operations = OB_OPERATION_HANDLE_CREATE;
        op_registration[0].PreOperation = PreObCallback;
        op_registration[0].PostOperation = nullptr;

        op_registration[1].ObjectType = PsThreadType;
        op_registration[1].Operations = OB_OPERATION_HANDLE_CREATE;
        op_registration[1].PreOperation = PreObCallback;
        op_registration[1].PostOperation = nullptr;

        ob_registration.OperationRegistration = op_registration;

        status = ObRegisterCallbacks(&ob_registration, &kHandleRegistration);
        if (!NT_SUCCESS(status))
        {
            DebugMessage("Fail to register ObRegisterCallbacks: %x", status);
        }
        else
        {
            DebugMessage("ObRegisterCallbacks success");
        }
    }

    // Huỷ đăng ký các callback bảo vệ process và thread
    void DrvUnload()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, TRUE);
        ObUnRegisterCallbacks(kHandleRegistration);

        kProcessMapMutex.Lock();
        delete kProcessMap;
        kProcessMapMutex.Unlock();
        kDirMutex.Lock();
        delete kProtectedDirList;
        kDirMutex.Unlock();
    }

    // Đăng ký các bộ lọc bảo vệ file
    void FltRegister()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        kEnableProtectFile = true;

        reg::kFltFuncVector->PushBack({ IRP_MJ_SET_INFORMATION, PreSetInformationFile, nullptr });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreCreateFile, nullptr });

        DebugMessage("protect_file FltRegister completed successfully.");
        return;
    }

    // Huỷ đăng ký các bộ lọc bảo vệ file
    void FltUnload()
    {
        DebugMessage("%ws", __FUNCTIONW__);
    }

    // Process notification callback
    VOID ProcessNotifyCallback(
        PEPROCESS process,
        HANDLE pid,
        PPS_CREATE_NOTIFY_INFO create_info
    )
    {
        if (create_info)
        {
            // Process is being created
            const String<WCHAR>& process_path = GetProcessImageName(pid);
            DebugMessage("%ws: creation, pid %d, path %ws", __FUNCTIONW__, (int)pid, process_path.Data());
            bool is_protected = IsInProtectedDirectory(process_path); // kiểm tra xem có cần bảo vệ không
            if (is_protected == false)
            {
                // If parent process is protected, then child process is also protected
                kProcessMapMutex.Lock();
                auto it = kProcessMap->Find(PsGetCurrentProcessId());
                if (it != kProcessMap->End())
                {
                    if (it->second.is_protected == true)
                    {
                        is_protected = true;
                    }
                }
                kProcessMapMutex.Unlock();
            }
			if (is_protected == true)
			{
				DebugMessage("Protected process %d: %ws", (int)pid, process_path.Data());
			}
            LARGE_INTEGER start_time;
            KeQuerySystemTime(&start_time);
            kProcessMapMutex.Lock();
            kProcessMap->Insert(pid, { pid, process_path, is_protected, start_time }); // lưu vào cache với trạng thái bảo vệ
            kProcessMapMutex.Unlock();
        }
        else
        {
            // Process kết thúc, xóa khỏi cache
            kProcessMapMutex.Lock();
            DebugMessage("%ws: termination, pid %d, killer %d, path %ws", __FUNCTIONW__, (int)pid, (int)PsGetCurrentProcessId(), GetProcessImageName(pid).Data());
            kProcessMap->Erase(pid);
            kProcessMapMutex.Unlock();
        }
    }

    // Kiểm tra quyền truy cập file
    FLT_PREOP_CALLBACK_STATUS PreCreateFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        if (kEnableProtectFile == false)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        if (data->RequestorMode == KernelMode)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        if (ExGetPreviousMode() == KernelMode)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
		auto pid = PsGetCurrentProcessId();
        if (IsProtectedProcess(pid))
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        String<WCHAR> file_path(flt::GetFileFullPathName(data));
		//DebugMessage("Create file: %ws\n", file_path.Data());
        if (IsInProtectedDirectory(file_path) == false)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

		DWORD desired_access = data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
        UINT8 create_disposition = data->Iopb->Parameters.Create.Options >> 24;
        DWORD create_options = data->Iopb->Parameters.Create.Options & 0x00FFFFFF;

        if (FlagOn(create_options, FILE_DELETE_ON_CLOSE)
            || !FlagOn(create_disposition, FILE_OPEN)
            || FlagOn(desired_access, FILE_WRITE_DATA)
            || FlagOn(desired_access, FILE_APPEND_DATA)
            //|| FlagOn(desired_access, FILE_WRITE_EA)
            //|| FlagOn(desired_access, FILE_WRITE_ATTRIBUTES)
            )
        {
            DebugMessage("Creation blocked, %ws, pid %d, process path %ws, desired_access 0x%x, create_disposition 0x%x, create_options %d", file_path.Data(), (int)PsGetCurrentProcessId(), GetProcessImageName(PsGetCurrentProcessId()).Data(), desired_access, create_disposition, create_options);
            data->IoStatus.Status = STATUS_ACCESS_DENIED;
            FltSetCallbackDataDirty(data);
            return FLT_PREOP_COMPLETE;
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Kiểm tra các quyền SetInformationFile
    FLT_PREOP_CALLBACK_STATUS PreSetInformationFile(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID* completion_context)
    {
        if (kEnableProtectFile == false)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
		if (data->RequestorMode == KernelMode)
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
        if (ExGetPreviousMode() == KernelMode)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }
        if (IsProtectedProcess(PsGetCurrentProcessId()))
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        String<WCHAR> file_path(flt::GetFileFullPathName(data));
		//DebugMessage("SetInformationFile: %ws", file_path.Data());
        if (IsInProtectedDirectory(file_path) == false)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        PFILE_RENAME_INFORMATION rename_info;
        PFLT_FILE_NAME_INFORMATION name_info;

        if (data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileAllocationInformation ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformationEx ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileEndOfFileInformation ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileShortNameInformation)
        {
            DebugMessage("SetInfo blocked, path %ws, pid %d, process path %ws, FileInformationClass %d", file_path.Data(), (int)PsGetCurrentProcessId(), GetProcessImageName(PsGetCurrentProcessId()).Data(), data->Iopb->Parameters.SetFileInformation.FileInformationClass);
            data->IoStatus.Status = STATUS_ACCESS_DENIED;
            FltSetCallbackDataDirty(data);
            return FLT_PREOP_COMPLETE;
        }

        if (data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileLinkInformation ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileLinkInformationEx ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformation ||
            data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileRenameInformationEx
            )
        {

            rename_info = (PFILE_RENAME_INFORMATION)data->Iopb->Parameters.SetFileInformation.InfoBuffer;

            NTSTATUS status = FltGetDestinationFileNameInformation(
                flt_objects->Instance,
                flt_objects->FileObject,
                rename_info->RootDirectory,
                rename_info->FileName,
                rename_info->FileNameLength,
                FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                &name_info
            );

            if (NT_SUCCESS(status))
            {
                //DebugMessage("Rename file: %wZ", name_info->Name);
                if (IsInProtectedDirectory(String<WCHAR>(name_info->Name))) {
                    DebugMessage("Rename blocked, path %ws, pid %d, process path %ws, FileInformationClass %d", file_path.Data(), (int)PsGetCurrentProcessId(), GetProcessImageName(PsGetCurrentProcessId()).Data(), data->Iopb->Parameters.SetFileInformation.FileInformationClass);
                    FltReleaseFileNameInformation(name_info);
                    data->IoStatus.Status = STATUS_ACCESS_DENIED;
                    FltSetCallbackDataDirty(data);
                    return FLT_PREOP_COMPLETE;
                }
                FltReleaseFileNameInformation(name_info);
            }

        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Bảo vệ process và thread qua callback
    OB_PREOP_CALLBACK_STATUS PreObCallback(PVOID registration_context, POB_PRE_OPERATION_INFORMATION operation_information)
    {
        if (operation_information->KernelHandle || ExGetPreviousMode() == KernelMode) return OB_PREOP_SUCCESS;
        
        HANDLE source_pid = PsGetCurrentProcessId();
        HANDLE target_pid = NULL; 
        if (operation_information->ObjectType == *PsThreadType)
        {
            target_pid = PsGetThreadProcessId((PETHREAD)operation_information->Object);
		}
		else if (operation_information->ObjectType == *PsProcessType)
		{
			target_pid = PsGetProcessId((PEPROCESS)operation_information->Object);
		}

        if (source_pid == target_pid)
        {
            return OB_PREOP_SUCCESS;
        }

        // Nếu source process là protected, cho phép hành động
        if (IsProtectedProcess(source_pid))
        {
            return OB_PREOP_SUCCESS;
        }

        // Nếu target process không được bảo vệ, cho phép hành động
        if (!IsProtectedProcess(target_pid))
        {
            return OB_PREOP_SUCCESS;
        }

        auto it = kProcessMap->Find(target_pid);
        if (it != kProcessMap->End())
        {
            // Lấy thời gian bắt đầu
            LARGE_INTEGER curr_time;
            KeQuerySystemTime(&curr_time);

            // Tính sự khác biệt giữa hai lần lấy thời gian (sự khác biệt theo đơn vị 100 ns)
            ULONG64 time_difference = curr_time.QuadPart - it->second.start_time.QuadPart;

            // Kiểm tra xem thời gian có nhỏ hơn 0.5 giây (500 triệu ns)
            if (time_difference < 5000000) // 5 triệu * 100 ns = 0.5 giây
            {
                return OB_PREOP_SUCCESS;
            }
        }

        // Kiểm tra loại đối tượng để xác định quyền cần sửa đổi
        if (operation_information->ObjectType == *PsProcessType)
        {
            // Xóa quyền như mô tả của Windows và thêm quyền cụ thể
            /*
            operation_information->Parameters->CreateHandleInformation.DesiredAccess &=
				~(DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER | PROCESS_ALL_ACCESS | PROCESS_TERMINATE |
                    PROCESS_CREATE_THREAD |
                    PROCESS_SET_INFORMATION |
                    PROCESS_VM_OPERATION | PROCESS_VM_WRITE);
            */
			DWORD desired_access = operation_information->Parameters->CreateHandleInformation.DesiredAccess;
            //DebugMessage("Desired access: %x", desired_access);
			if (desired_access & PROCESS_TERMINATE)
			{
                //DebugMessage("Terminate blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~PROCESS_TERMINATE;
			}
            if (desired_access & PROCESS_CREATE_THREAD)
            {
                //DebugMessage("Create thread blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~PROCESS_CREATE_THREAD;
            }
			if (desired_access & PROCESS_SET_SESSIONID)
			{
				//DebugMessage("Set sessionid blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
				desired_access &= ~PROCESS_SET_SESSIONID;
			}
            if (desired_access & PROCESS_VM_OPERATION)
            {
                //DebugMessage("VM operation blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~PROCESS_VM_OPERATION;
            }
            if (desired_access & PROCESS_VM_WRITE)
            {
                //DebugMessage("VM write blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~PROCESS_VM_WRITE;
            }
            if (desired_access & PROCESS_SET_INFORMATION)
            {
                //DebugMessage("Set information blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~PROCESS_SET_INFORMATION;
            }
            if (desired_access & DELETE)
            {
                //DebugMessage("Delete blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
                desired_access &= ~DELETE;
            }
			if (desired_access & WRITE_DAC)
			{
                //DebugMessage("Write DAC blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
				desired_access &= ~WRITE_DAC;
			}
			if (desired_access & WRITE_OWNER)
			{
                //DebugMessage("Write owner blocked, source %lld, target %lld", (size_t)source_pid, (size_t)target_pid);
				desired_access &= ~WRITE_OWNER;
			}
			operation_information->Parameters->CreateHandleInformation.DesiredAccess = desired_access;
        }
        else if (operation_information->ObjectType == *PsThreadType)
        {
            // Xóa các quyền cần thiết đối với thread
            operation_information->Parameters->CreateHandleInformation.DesiredAccess &=
                ~(THREAD_DIRECT_IMPERSONATION | THREAD_IMPERSONATE | THREAD_SET_CONTEXT |
                    THREAD_SET_INFORMATION | THREAD_SET_THREAD_TOKEN | THREAD_SUSPEND_RESUME |
                    THREAD_TERMINATE | WRITE_DAC | WRITE_OWNER);
        }

        return OB_PREOP_SUCCESS;
    }

    // Kiểm tra xem thư mục có nằm trong danh sách bảo vệ không
    bool IsInProtectedDirectory(const String<WCHAR>& path)
    {
        kDirMutex.Lock();
        bool is_protected = false;

        for (int i = 0; i < kProtectedDirList->Size(); i++) {
            const auto& protected_dir = kProtectedDirList->At(i);
            if (protected_dir.IsPrefixOf(path)) {
                is_protected = true;
                break;
            }
        }

        kDirMutex.Unlock();
        return is_protected;
    }

    // Kiểm tra xem PID có thuộc process cần bảo vệ không
    bool IsProtectedProcess(HANDLE pid)
    {
        kProcessMapMutex.Lock();
        auto it = kProcessMap->Find(pid);
        bool is_protected = false;

        if (it != kProcessMap->End())
        {
            is_protected = it->second.is_protected; // lấy trạng thái bảo vệ từ cache
            if (is_protected == false)
            {
                is_protected = IsInProtectedDirectory(it->second.process_path);
            }
        }
        else
        {
            // Process không có trong cache, lấy thông tin mới
            String<WCHAR> process_path = GetProcessImageName(pid);
            is_protected = IsInProtectedDirectory(process_path);

            // Lưu vào cache
            kProcessMap->Insert(pid, { pid, process_path, is_protected, 0 });
        }
        kProcessMapMutex.Unlock();

        return is_protected;
    }

    String<WCHAR> GetProcessImageName(HANDLE pid)
    {
        auto it = kProcessMap->Find(pid);
        if (it != kProcessMap->End())
        {
            return it->second.process_path;
        }

        String<WCHAR> process_image_name;
        NTSTATUS status = STATUS_UNSUCCESSFUL;
        PEPROCESS eproc;
        status = PsLookupProcessByProcessId(pid, &eproc);
        if (!NT_SUCCESS(status))
        {
            DebugMessage("PsLookupProcessByProcessId for pid %p Failed: %08x\n", pid, status);
            return String<WCHAR>();
        }

        PUNICODE_STRING process_name = NULL;
        status = SeLocateProcessImageName(eproc, &process_name);
        if (status == STATUS_SUCCESS)
        {
            process_image_name = process_name;
            delete process_name;
            ObDereferenceObject(eproc);
            return process_image_name;
        }

        ObDereferenceObject(eproc);

        ULONG returned_length = 0;
        HANDLE h_process = NULL;
        Vector<UCHAR> buffer;

        if (ZwQueryInformationProcess == NULL)
        {
            DebugMessage("Cannot resolve ZwQueryInformationProcess\n");
            status = STATUS_UNSUCCESSFUL;
            goto cleanUp;
        }

        status = ObOpenObjectByPointer(eproc,
            0, NULL, 0, 0, KernelMode, &h_process);
        if (!NT_SUCCESS(status))
        {
            DebugMessage("ObOpenObjectByPointer Failed: %08x\n", status);
            return String<WCHAR>();
        }

        if (ZwQueryInformationProcess == NULL)
        {
            DebugMessage("Cannot resolve ZwQueryInformationProcess\n");
            status = STATUS_UNSUCCESSFUL;
            goto cleanUp;
        }

        status = ZwQueryInformationProcess(h_process,
            ProcessImageFileName,
            NULL,
            0,
            &returned_length);

        if (STATUS_INFO_LENGTH_MISMATCH != status) {
            DebugMessage("ZwQueryInformationProcess status = %x\n", status);
            goto cleanUp;
        }

        buffer.Resize(returned_length);

        if (buffer.Data() == NULL)
        {
            goto cleanUp;
        }

        status = ZwQueryInformationProcess(h_process,
            ProcessImageFileName,
            buffer.Data(),
            returned_length,
            &returned_length);
        process_image_name = (PUNICODE_STRING)(buffer.Data());

    cleanUp:
        if (h_process != NULL)
        {
            ZwClose(h_process);
        }

        DebugMessage("GetProcessImageName: %ws", process_image_name.Data());
        return process_image_name;
    }

    String<WCHAR> GetDefaultProtectedDir()
    {
        UNICODE_STRING protected_dir;
        RtlInitUnicodeString(&protected_dir, L"\\??\\E:\\");
        UNICODE_STRING protected_dir_device_path;
        protected_dir_device_path.Length = 0;
        protected_dir_device_path.MaximumLength = 1024 * sizeof(WCHAR);
        protected_dir_device_path.Buffer = new WCHAR[1024];
        NormalizeDevicePath(&protected_dir, &protected_dir_device_path);
        String<WCHAR> protected_dir_device_path_str(protected_dir_device_path);
        delete protected_dir_device_path.Buffer;
        return protected_dir_device_path_str;
    }

    NTSTATUS ResolveSymbolicLink(PUNICODE_STRING Link, PUNICODE_STRING Resolved)
    {
        OBJECT_ATTRIBUTES attribs;
        HANDLE hsymLink;
        ULONG written;
        NTSTATUS status = STATUS_SUCCESS;

        // Open symlink

        InitializeObjectAttributes(&attribs, Link, OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwOpenSymbolicLinkObject(&hsymLink, GENERIC_READ, &attribs);
        if (!NT_SUCCESS(status))
            return status;

        // Query original name

        status = ZwQuerySymbolicLinkObject(hsymLink, Resolved, &written);
        ZwClose(hsymLink);
        if (!NT_SUCCESS(status))
            return status;

        return status;
    }

    //
    // Convertion template:
    //   \\??\\C:\\Windows -> \\Device\\HarddiskVolume1\\Windows
    //
    NTSTATUS NormalizeDevicePath(PCUNICODE_STRING Path, PUNICODE_STRING Normalized)
    {
        UNICODE_STRING globalPrefix, dvcPrefix, sysrootPrefix;
        NTSTATUS status;

        RtlInitUnicodeString(&globalPrefix, L"\\??\\");
        RtlInitUnicodeString(&dvcPrefix, L"\\Device\\");
        RtlInitUnicodeString(&sysrootPrefix, L"\\SystemRoot\\");

        if (RtlPrefixUnicodeString(&globalPrefix, Path, TRUE))
        {
            OBJECT_ATTRIBUTES attribs;
            UNICODE_STRING subPath;
            HANDLE hsymLink;
            ULONG i, written, size;

            subPath.Buffer = (PWCH)((PUCHAR)Path->Buffer + globalPrefix.Length);
            subPath.Length = Path->Length - globalPrefix.Length;

            for (i = 0; i < subPath.Length; i++)
            {
                if (subPath.Buffer[i] == L'\\')
                {
                    subPath.Length = (USHORT)(i * sizeof(WCHAR));
                    break;
                }
            }

            if (subPath.Length == 0)
                return STATUS_INVALID_PARAMETER_1;

            subPath.Buffer = Path->Buffer;
            subPath.Length += globalPrefix.Length;
            subPath.MaximumLength = subPath.Length;

            // Open symlink

            InitializeObjectAttributes(&attribs, &subPath, OBJ_KERNEL_HANDLE, NULL, NULL);

            status = ZwOpenSymbolicLinkObject(&hsymLink, GENERIC_READ, &attribs);
            if (!NT_SUCCESS(status))
                return status;

            // Query original name

            status = ZwQuerySymbolicLinkObject(hsymLink, Normalized, &written);
            ZwClose(hsymLink);
            if (!NT_SUCCESS(status))
                return status;

            // Construct new variable

            size = Path->Length - subPath.Length + Normalized->Length;
            if (size > Normalized->MaximumLength)
                return STATUS_BUFFER_OVERFLOW;

            subPath.Buffer = (PWCH)((PUCHAR)Path->Buffer + subPath.Length);
            subPath.Length = Path->Length - subPath.Length;
            subPath.MaximumLength = subPath.Length;

            status = RtlAppendUnicodeStringToString(Normalized, &subPath);
            if (!NT_SUCCESS(status))
                return status;
        }
        else if (RtlPrefixUnicodeString(&dvcPrefix, Path, TRUE))
        {
            Normalized->Length = 0;
            status = RtlAppendUnicodeStringToString(Normalized, Path);
            if (!NT_SUCCESS(status))
                return status;
        }
        else if (RtlPrefixUnicodeString(&sysrootPrefix, Path, TRUE))
        {
            UNICODE_STRING subPath, resolvedLink, winDir;
            WCHAR buffer[64];
            SHORT i;

            // Open symlink

            subPath.Buffer = sysrootPrefix.Buffer;
            subPath.MaximumLength = subPath.Length = sysrootPrefix.Length - sizeof(WCHAR);

            resolvedLink.Buffer = buffer;
            resolvedLink.Length = 0;
            resolvedLink.MaximumLength = sizeof(buffer);

            status = ResolveSymbolicLink(&subPath, &resolvedLink);
            if (!NT_SUCCESS(status))
                return status;

            // \Device\Harddisk0\Partition0\Windows -> \Device\Harddisk0\Partition0
            // Win10: \Device\BootDevice\Windows -> \Device\BootDevice

            winDir.Length = 0;
            for (i = (resolvedLink.Length - sizeof(WCHAR)) / sizeof(WCHAR); i >= 0; i--)
            {
                if (resolvedLink.Buffer[i] == L'\\')
                {
                    winDir.Buffer = resolvedLink.Buffer + i;
                    winDir.Length = resolvedLink.Length - (i * sizeof(WCHAR));
                    winDir.MaximumLength = winDir.Length;
                    resolvedLink.Length = (i * sizeof(WCHAR));
                    break;
                }
            }

            // \Device\Harddisk0\Partition0 -> \Device\HarddiskVolume1
            // Win10: \Device\BootDevice -> \Device\HarddiskVolume2

            status = ResolveSymbolicLink(&resolvedLink, Normalized);
            if (!NT_SUCCESS(status))
                return status;

            // Construct new variable

            subPath.Buffer = (PWCHAR)((PCHAR)Path->Buffer + sysrootPrefix.Length - sizeof(WCHAR));
            subPath.MaximumLength = subPath.Length = Path->Length - sysrootPrefix.Length + sizeof(WCHAR);

            status = RtlAppendUnicodeStringToString(Normalized, &winDir);
            if (!NT_SUCCESS(status))
                return status;

            status = RtlAppendUnicodeStringToString(Normalized, &subPath);
            if (!NT_SUCCESS(status))
                return status;
        }
        else
        {
            return STATUS_INVALID_PARAMETER;
        }

        return STATUS_SUCCESS;
    }
} // namespace self_defense
