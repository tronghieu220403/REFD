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
    static Vector<String<WCHAR>>* kProtectedFileList;
    static Mutex kFileMutex;
    static PVOID kHandleRegistration;
    static bool kEnableProtectFile;

    void DrvRegister()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        kFileMutex.Create();
        kProcessMapMutex.Create();

		kFileMutex.Lock();
        kProtectedDirList = new Vector<String<WCHAR>>();
		Vector<String<WCHAR>> default_protected_dirs = GetDefaultProtectedDirs();
		for (int i = 0; i < default_protected_dirs.Size(); ++i)
		{
			kProtectedDirList->PushBack(default_protected_dirs[i]);
		}

		kProtectedFileList = new Vector<String<WCHAR>>();
		Vector<String<WCHAR>> default_protected_files = GetDefaultProtectedFiles();
		for (int i = 0; i < default_protected_files.Size(); ++i)
		{
			kProtectedFileList->PushBack(default_protected_files[i]);
		}
		kFileMutex.Unlock();

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

    void DrvUnload()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallback, TRUE);
        ObUnRegisterCallbacks(kHandleRegistration);

        kProcessMapMutex.Lock();
        delete kProcessMap;
        kProcessMapMutex.Unlock();
        kFileMutex.Lock();
        delete kProtectedDirList;
        kFileMutex.Unlock();
    }

    void FltRegister()
    {
        DebugMessage("%ws", __FUNCTIONW__);
        kEnableProtectFile = true;

        reg::kFltFuncVector->PushBack({ IRP_MJ_SET_INFORMATION, PreSetInformationFile, nullptr });
        reg::kFltFuncVector->PushBack({ IRP_MJ_CREATE, PreCreateFile, nullptr });

        DebugMessage("FltRegister completed successfully.");
        return;
    }

    NTSTATUS FltUnload()
    {
        DebugMessage("Begin %ws", __FUNCTIONW__);
		HANDLE pid = PsGetCurrentProcessId();
		if (pid == (HANDLE)4 || ExGetPreviousMode() == KernelMode)
		{
			return STATUS_SUCCESS;
		}
		kProcessMapMutex.Lock();
		const String<WCHAR>& process_path = GetProcessImageName(pid);
		kProcessMapMutex.Unlock();
		if (IsProtectedFile(process_path))
		{
			return STATUS_SUCCESS;
		}
		return STATUS_FLT_DO_NOT_DETACH;
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
			auto ppid = PsGetCurrentProcessId();
            const String<WCHAR>& process_path = GetProcessImageName(pid);
            DebugMessage("Creation, pid %llu, path %ws", (ull)pid, process_path.Data());
            bool is_protected = IsProtectedFile(process_path); // kiểm tra xem có cần bảo vệ không
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
				else
				{
					is_protected = IsProtectedFile(GetProcessImageName(ppid));
				}
                kProcessMapMutex.Unlock();
            }
			if (is_protected == true)
			{
				DebugMessage("Protected process %llu: %ws", (ull)pid, process_path.Data());
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
            DebugMessage("%ws: termination, pid %llu, killer %llu, path %ws", __FUNCTIONW__, (ull)pid, (ull)PsGetCurrentProcessId(), GetProcessImageName(pid).Data());
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

        String<WCHAR> file_path(flt::GetFileFullPathName(data));
        if (file_path.Size() == 0 || IsProtectedFile(file_path) == false)
        {
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

		auto pid = PsGetCurrentProcessId();
		if (IsProtectedProcess(pid))
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}

		DWORD desired_access = data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
        UINT8 create_disposition = data->Iopb->Parameters.Create.Options >> 24;
        DWORD create_options = data->Iopb->Parameters.Create.Options & 0x00FFFFFF;

        if (FlagOn(create_options, FILE_DELETE_ON_CLOSE)
            || create_disposition != FILE_OPEN
            || FlagOn(desired_access, FILE_WRITE_DATA)
            || FlagOn(desired_access, FILE_APPEND_DATA)
            || FlagOn(desired_access, DELETE)
            //|| FlagOn(desired_access, FILE_WRITE_EA)
            //|| FlagOn(desired_access, FILE_WRITE_ATTRIBUTES)
            )
        {
            DebugMessage("Creation blocked, %ws, pid %llu, process path %ws, desired_access 0x%x, create_disposition 0x%x, create_options %d", file_path.Data(), (ull)PsGetCurrentProcessId(), GetProcessImageName(PsGetCurrentProcessId()).Data(), desired_access, create_disposition, create_options);
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
        if (IsProtectedFile(file_path) == false)
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
            DebugMessage("SetInfo blocked, path %ws, pid %llu, process path %ws, FileInformationClass %d", file_path.Data(), (ull)PsGetCurrentProcessId(), GetProcessImageName(PsGetCurrentProcessId()).Data(), data->Iopb->Parameters.SetFileInformation.FileInformationClass);
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
                if (IsProtectedFile(String<WCHAR>(name_info->Name))) {
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
			//DebugMessage("PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
			if (desired_access & PROCESS_TERMINATE)
			{
				DebugMessage("Terminate blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_TERMINATE;
			}
			if (desired_access & PROCESS_CREATE_THREAD)
			{
				DebugMessage("Create thread blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_CREATE_THREAD;
			}
			if (desired_access & PROCESS_SET_SESSIONID)
			{
				DebugMessage("Set sessionid blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_SET_SESSIONID;
			}
			if (desired_access & PROCESS_VM_OPERATION)
			{
				DebugMessage("VM operation blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_VM_OPERATION;
			}
			if (desired_access & PROCESS_VM_WRITE)
			{
				DebugMessage("VM write blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_VM_WRITE;
			}
			if (desired_access & PROCESS_SET_INFORMATION)
			{
				DebugMessage("Set information blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~PROCESS_SET_INFORMATION;
			}
			if (desired_access & DELETE)
			{
				DebugMessage("Delete blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~DELETE;
			}
			if (desired_access & WRITE_DAC)
			{
				DebugMessage("Write DAC blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
				desired_access &= ~WRITE_DAC;
			}
			if (desired_access & WRITE_OWNER)
			{
				DebugMessage("Write owner blocked, PID %llu -> PID %llu, desired access 0x%x (%ws -> %ws)", (ull)source_pid, (ull)target_pid, desired_access, GetProcessImageName(source_pid).Data(), GetProcessImageName(target_pid).Data());
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
	bool IsProtectedFile(const String<WCHAR>& path)
	{
		//DebugMessage("Checking file: %ws", path.Data());
		kFileMutex.Lock();
		bool is_protected = false;

		for (int i = 0; i < kProtectedDirList->Size(); i++) {
			const auto& protected_dir = kProtectedDirList->At(i);
			/*
			if (protected_dir.IsPrefixOf(path)) {
				is_protected = true;
				break;
			}
			*/
			if (protected_dir.Size() < path.Size() && _wcsnicmp(protected_dir.Data(), path.Data(), protected_dir.Size()) == 0)
			{
				is_protected = true;
			}
        }

		for (int i = 0; i < kProtectedFileList->Size(); i++) {
			const auto& protected_file = kProtectedFileList->At(i);
			/*
			if (protected_file.IsPrefixOf(path) && path.IsPrefixOf(protected_file)) {
				is_protected = true;
				break;
			}
			*/
			if (protected_file.Size() == path.Size() && _wcsnicmp(protected_file.Data(), path.Data(), protected_file.Size()) == 0)
			{
				is_protected = true;
			}
		}

        kFileMutex.Unlock();

		if (is_protected)
		{
			DebugMessage("File %ws is protected", path.Data());
		}
		else
		{
			//DebugMessage("File %ws is not protected", path.Data());
		}

        return is_protected;
    }

	bool IsInProtectedFile(const String<WCHAR>& path)
	{
		return false;
	}

    // Kiểm tra xem PID có thuộc process cần bảo vệ không
    bool IsProtectedProcess(HANDLE pid)
    {
        kProcessMapMutex.Lock();
		DebugMessage("Checking pid %llu: %ws", (ull)pid, GetProcessImageName(pid).Data());

        auto it = kProcessMap->Find(pid);
        bool is_protected = false;

        if (it != kProcessMap->End())
        {
			DebugMessage("PID %llu is in process map", (ull)pid);
            is_protected = it->second.is_protected; // lấy trạng thái bảo vệ từ cache
            if (is_protected == false)
            {
                is_protected = IsProtectedFile(it->second.process_path);
            }
        }
        else
        {
			DebugMessage("PID %llu is not in process map", (ull)pid);

            // Process không có trong cache, lấy thông tin mới
            String<WCHAR> process_path = GetProcessImageName(pid);
            is_protected = IsProtectedFile(process_path);

            // Lưu vào cache
            kProcessMap->Insert(pid, { pid, process_path, is_protected, 0 });
        }
        kProcessMapMutex.Unlock();

		if (is_protected)
		{
			DebugMessage("Pid %llu is protected", (ull)pid);
		}
		else
		{
			DebugMessage("Pid %llu is not protected", (ull)pid);
		}

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

	const WCHAR* kDevicePathDirList[] = {
		L"\\??\\E:\\",
		L"\\??\\C:\\Program Files\\VMware\\VMware Tools\\"
	};

	Vector<String<WCHAR>> GetDefaultProtectedDirs()
    {
		Vector<String<WCHAR>> protected_dirs;
		for (int i = 0; i < sizeof(kDevicePathDirList) / sizeof(kDevicePathDirList[0]); ++i) {
			UNICODE_STRING device_path_uni_str;
			RtlInitUnicodeString(&device_path_uni_str, kDevicePathDirList[i]);
			String<WCHAR> nomalized_path_str;
			nomalized_path_str.Resize(1024);
			UNICODE_STRING normalized_uni_str = { 0, nomalized_path_str.Size() * sizeof(WCHAR), nomalized_path_str.Data() };
			NormalizeDevicePath(&device_path_uni_str, &normalized_uni_str);
			nomalized_path_str.Resize(normalized_uni_str.Length / sizeof(WCHAR));

			protected_dirs.PushBack(nomalized_path_str);

			DebugMessage("Protected dir: %ws", nomalized_path_str.Data());

		}
		return protected_dirs;
	}

	const WCHAR* kDevicePathFileList[] = {
		L"\\??\\C:\\Windows\\System32\\drivers\\SelfDefenseKernel.sys",
		L"\\??\\C:\\Windows\\System32\\drivers\\EventCollectorDriver.sys",
		L"\\Device\\Harddisk0\\DR0"
	};

	Vector<String<WCHAR>> GetDefaultProtectedFiles()
	{
		Vector<String<WCHAR>> protected_files;
		for (int i = 0; i < sizeof(kDevicePathFileList) / sizeof(kDevicePathFileList[0]); ++i) {
			UNICODE_STRING device_path_uni_str;
			RtlInitUnicodeString(&device_path_uni_str, kDevicePathFileList[i]);
			String<WCHAR> nomalized_path_str;
			nomalized_path_str.Resize(1024);
			UNICODE_STRING normalized_uni_str = { 0, nomalized_path_str.Size() * sizeof(WCHAR), nomalized_path_str.Data() };
			NormalizeDevicePath(&device_path_uni_str, &normalized_uni_str);
			nomalized_path_str.Resize(normalized_uni_str.Length / sizeof(WCHAR));

			protected_files.PushBack(nomalized_path_str);
			DebugMessage("Protected file: %ws", nomalized_path_str.Data());

		}
		return protected_files;
	}

    NTSTATUS ResolveSymbolicLink(const PUNICODE_STRING& link, const PUNICODE_STRING& resolved)
    {
        OBJECT_ATTRIBUTES attribs;
        HANDLE hsymLink;
        ULONG written;
        NTSTATUS status = STATUS_SUCCESS;

        // Open symlink

        InitializeObjectAttributes(&attribs, link, OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwOpenSymbolicLinkObject(&hsymLink, GENERIC_READ, &attribs);
        if (!NT_SUCCESS(status))
            return status;

        // Query original name

        status = ZwQuerySymbolicLinkObject(hsymLink, resolved, &written);
        ZwClose(hsymLink);
        if (!NT_SUCCESS(status))
            return status;

        return status;
    }

    //
    // Convertion template:
    //   \\??\\C:\\Windows -> \\Device\\HarddiskVolume1\\Windows
    //
    NTSTATUS NormalizeDevicePath(const PCUNICODE_STRING& path, const PUNICODE_STRING& normalized)
    {
        UNICODE_STRING global_prefix, dvc_prefix, sysroot_prefix;
        NTSTATUS status;

        RtlInitUnicodeString(&global_prefix, L"\\??\\");
        RtlInitUnicodeString(&dvc_prefix, L"\\Device\\");
        RtlInitUnicodeString(&sysroot_prefix, L"\\SystemRoot\\");

        if (RtlPrefixUnicodeString(&global_prefix, path, TRUE))
        {
            OBJECT_ATTRIBUTES attribs;
            UNICODE_STRING sub_Path;
            HANDLE hsym_link;
            ULONG i, written, size;

            sub_Path.Buffer = (PWCH)((PUCHAR)path->Buffer + global_prefix.Length);
            sub_Path.Length = path->Length - global_prefix.Length;

            for (i = 0; i < sub_Path.Length; i++)
            {
                if (sub_Path.Buffer[i] == L'\\')
                {
                    sub_Path.Length = (USHORT)(i * sizeof(WCHAR));
                    break;
                }
            }

            if (sub_Path.Length == 0)
                return STATUS_INVALID_PARAMETER_1;

            sub_Path.Buffer = path->Buffer;
            sub_Path.Length += global_prefix.Length;
            sub_Path.MaximumLength = sub_Path.Length;

            // Open symlink

            InitializeObjectAttributes(&attribs, &sub_Path, OBJ_KERNEL_HANDLE, NULL, NULL);

            status = ZwOpenSymbolicLinkObject(&hsym_link, GENERIC_READ, &attribs);
            if (!NT_SUCCESS(status))
                return status;

            // Query original name

            status = ZwQuerySymbolicLinkObject(hsym_link, normalized, &written);
            ZwClose(hsym_link);
            if (!NT_SUCCESS(status))
                return status;

            // Construct new variable

            size = path->Length - sub_Path.Length + normalized->Length;
            if (size > normalized->MaximumLength)
                return STATUS_BUFFER_OVERFLOW;

            sub_Path.Buffer = (PWCH)((PUCHAR)path->Buffer + sub_Path.Length);
            sub_Path.Length = path->Length - sub_Path.Length;
            sub_Path.MaximumLength = sub_Path.Length;

            status = RtlAppendUnicodeStringToString(normalized, &sub_Path);
            if (!NT_SUCCESS(status))
                return status;
        }
        else if (RtlPrefixUnicodeString(&dvc_prefix, path, TRUE))
        {
            normalized->Length = 0;
            status = RtlAppendUnicodeStringToString(normalized, path);
            if (!NT_SUCCESS(status))
                return status;
        }
        else if (RtlPrefixUnicodeString(&sysroot_prefix, path, TRUE))
        {
            UNICODE_STRING sub_path, resolved_link, win_dir;
            WCHAR buffer[64];
            SHORT i;

            // Open symlink

            sub_path.Buffer = sysroot_prefix.Buffer;
            sub_path.MaximumLength = sub_path.Length = sysroot_prefix.Length - sizeof(WCHAR);

            resolved_link.Buffer = buffer;
            resolved_link.Length = 0;
            resolved_link.MaximumLength = sizeof(buffer);

            status = ResolveSymbolicLink(&sub_path, &resolved_link);
            if (!NT_SUCCESS(status))
                return status;

            // \Device\Harddisk0\Partition0\Windows -> \Device\Harddisk0\Partition0
            // Win10: \Device\BootDevice\Windows -> \Device\BootDevice

            win_dir.Length = 0;
            for (i = (resolved_link.Length - sizeof(WCHAR)) / sizeof(WCHAR); i >= 0; i--)
            {
                if (resolved_link.Buffer[i] == L'\\')
                {
                    win_dir.Buffer = resolved_link.Buffer + i;
                    win_dir.Length = resolved_link.Length - (i * sizeof(WCHAR));
                    win_dir.MaximumLength = win_dir.Length;
                    resolved_link.Length = (i * sizeof(WCHAR));
                    break;
                }
            }

            // \Device\Harddisk0\Partition0 -> \Device\HarddiskVolume1
            // Win10: \Device\BootDevice -> \Device\HarddiskVolume2

            status = ResolveSymbolicLink(&resolved_link, normalized);
            if (!NT_SUCCESS(status))
                return status;

            // Construct new variable

            sub_path.Buffer = (PWCHAR)((PCHAR)path->Buffer + sysroot_prefix.Length - sizeof(WCHAR));
            sub_path.MaximumLength = sub_path.Length = path->Length - sysroot_prefix.Length + sizeof(WCHAR);

            status = RtlAppendUnicodeStringToString(normalized, &win_dir);
            if (!NT_SUCCESS(status))
                return status;

            status = RtlAppendUnicodeStringToString(normalized, &sub_path);
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
