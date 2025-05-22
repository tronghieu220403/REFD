#include "file.h"
#include <Fltkernel.h>

namespace file
{
	ZwFile::ZwFile(const String<WCHAR>& current_path) :
		file_path_(current_path)
	{

	}

	NTSTATUS ZwFile::Open(const WCHAR* file_path, ULONG create_disposition)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}

		UNICODE_STRING uni_str;
		OBJECT_ATTRIBUTES obj_attr;
		IO_STATUS_BLOCK io_status_block;

		RtlInitUnicodeString(&uni_str, file_path);
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		NTSTATUS status = ZwCreateFile(&file_handle_, FILE_ALL_ACCESS, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, 0, create_disposition, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
		return status;
	}

	ull ZwFile::Read(PVOID buffer, ull length)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		IO_STATUS_BLOCK io_status_block;
		if (!NT_SUCCESS(ZwReadFile(file_handle_, NULL, NULL, NULL, &io_status_block, buffer, (ULONG)length, NULL, NULL)))
		{
			return 0;
		}
		return io_status_block.Information;
	}

	ull ZwFile::Append(PVOID buffer, ull length)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		IO_STATUS_BLOCK io_status_block;

		LARGE_INTEGER li_byte_offset;
		li_byte_offset.HighPart = -1;
		li_byte_offset.LowPart = FILE_WRITE_TO_END_OF_FILE;

		if (!NT_SUCCESS(ZwWriteFile(file_handle_, NULL, NULL, NULL, &io_status_block, buffer, (ULONG)length, &li_byte_offset, NULL)))
		{
			return 0;
		}
		return io_status_block.Information;
	}

	ull ZwFile::Size()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return 0;
		}
		IO_STATUS_BLOCK io_status_block;
		FILE_STANDARD_INFORMATION file_info;

		if (!NT_SUCCESS(ZwQueryInformationFile(file_handle_, &io_status_block, &file_info, sizeof(file_info), FileStandardInformation)))
		{
			return 0;
		}
		return file_info.EndOfFile.QuadPart;
	}


	void ZwFile::Close() {
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return;
		}
		if (file_handle_ == nullptr)
		{
			return;
		}
		ZwClose(file_handle_);
		file_handle_ = nullptr;
	}

	ZwFile::~ZwFile()
	{
		Close();
	}

	FileFlt::FileFlt(const String<WCHAR>& current_path, const PFLT_FILTER p_filter_handle, const PFLT_INSTANCE p_instance, const PFILE_OBJECT p_file_object, ULONG create_disposition) :
		file_path_(current_path),
		p_filter_handle_(p_filter_handle),
		p_instance_(p_instance),
		p_file_object_(p_file_object),
		create_disposition_(create_disposition)
	{
		if (p_file_object_ != nullptr)
		{
			pre_alloc_file_object_ = true;
		}
	}

	void FileFlt::SetPfltFilter(PFLT_FILTER p_filter_handle)
	{
		p_filter_handle_ = p_filter_handle;
	}

	NTSTATUS FileFlt::Open()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		if (pre_alloc_file_object_ == true)
		{
			is_open_ = true;
			return true;
		}
		UNICODE_STRING uni_str = { file_path_.Size() * sizeof(WCHAR), file_path_.Size() * sizeof(WCHAR), file_path_.Data() };
		OBJECT_ATTRIBUTES obj_attr;
		IO_STATUS_BLOCK io_status_block;
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		NTSTATUS status = FltCreateFileEx(p_filter_handle_, p_instance_, &file_handle_, &p_file_object_, FILE_GENERIC_READ | FILE_GENERIC_WRITE, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, create_disposition_, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0, IO_IGNORE_SHARE_ACCESS_CHECK);
		if (NT_SUCCESS(status))
		{
			is_open_ = true;
		}
		else
		{
			//DebugMessage("FltCreateFile %ws failed: %x", file_path_.Data(), status);
			is_open_ = false;
			file_handle_ = nullptr;
			p_file_object_ = nullptr;
		}
		return status;
	}

	ull FileFlt::Read(PVOID buffer, ull length)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return 0;
		}
		if (is_open_ == false)
		{
			return 0;
		}
		ULONG bytes_read = 0;

		NTSTATUS status = FltReadFile(p_instance_, p_file_object_, NULL, (ULONG)length, buffer, FLTFL_IO_OPERATION_NON_CACHED, &bytes_read, nullptr, nullptr);
		if (!NT_SUCCESS(status))
		{
			DebugMessage("FltReadFile failed: %x", status);
			return 0;
		}
		return bytes_read;
	}

	ull FileFlt::ReadWithOffset(PVOID buffer, ull length, ull offset)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return 0;
		}
		if (is_open_ == false)
		{
			return 0;
		}

		ULONG bytes_read = 0;

		// Word around to avoid STATUS_INVALID_OFFSET_ALIGNMENT (0xC0000474) error.
		ull new_length;
		PVOID new_buffer;
		ull new_offset = offset / 4 * 4;
		if (new_offset < offset)
		{
			new_length = length + (offset - new_offset);
			new_buffer = new UCHAR[new_length];
		}
		else
		{
			new_offset = offset;
			new_length = length;
			new_buffer = buffer;
		}

		LARGE_INTEGER byte_offset;
		byte_offset.QuadPart = new_offset;
		// If FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET and FLTFL_IO_OPERATION_NON_CACHED are only set, the function will hang forever.

		NTSTATUS status = FltReadFile(p_instance_, p_file_object_, &byte_offset, (ULONG)new_length, new_buffer, FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET | FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_PAGING | FLTFL_IO_OPERATION_SYNCHRONOUS_PAGING, &bytes_read, nullptr, nullptr);
		if (!NT_SUCCESS(status))
		{
			if (status == STATUS_END_OF_FILE)
			{
				//DebugMessage("FltReadFile read %u bytes, exptected %llu bytes", bytes_read, new_length);
				if (bytes_read == 0)
				{
					return 0;
				}
			}
			else
			{
				//DebugMessage("FltReadFile failed: %x", status);
				return 0;
			}
		}

		// Copy the data from new_buffer to the old buffer
		if (new_offset < offset)
		{
			memcpy(buffer, (PUCHAR)new_buffer + (offset - new_offset), bytes_read - (offset - new_offset));
			delete[] new_buffer;
		}
		return bytes_read - (offset - new_offset);
	}

	bool FileFlt::Append(PVOID buffer, ull length)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		ULONG bytes_written = 0;
		// The caller can set ByteOffset->LowPart to FILE_WRITE_TO_END_OF_FILE and ByteOffset->HighPart to -1 to start the write at the end of the file (i.e., perform an appending write).
		LARGE_INTEGER byte_offset;
		byte_offset.LowPart = FILE_WRITE_TO_END_OF_FILE;
		byte_offset.HighPart = -1;

		NTSTATUS status = FltWriteFile(p_instance_, p_file_object_, &byte_offset, (ULONG)length, buffer, NULL, &bytes_written, nullptr, nullptr);
		if (!NT_SUCCESS(status))
		{
			//DebugMessage("FltWriteFile failed: %x", status);
			return false;
		}
		return NT_SUCCESS(status);
	}

	bool FileFlt::Exist()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		HANDLE file_handle_tmp = nullptr;
		UNICODE_STRING uni_str = { file_path_.Size() * sizeof(WCHAR), file_path_.Size() * sizeof(WCHAR), file_path_.Data() };
		OBJECT_ATTRIBUTES obj_attr;
		IO_STATUS_BLOCK io_status_block = { 0 };
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		NTSTATUS status = FltCreateFile(p_filter_handle_, p_instance_, &file_handle_tmp, FILE_READ_ATTRIBUTES, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0, IO_IGNORE_SHARE_ACCESS_CHECK);
		if (NT_SUCCESS(status))
		{
			FltClose(file_handle_tmp);
			return true;
		}
		else if (status == STATUS_SHARING_VIOLATION)
		{
			return true;
		}
		//DebugMessage("FltCreateFile %ws returned error: %x", file_path_.Data(), status);
		return false;
	}

	ull FileFlt::Size()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			//DebugMessage("KeGetCurrentIrql != PASSIVE_LEVEL");
			return ULL_MAX;
		}
		if (is_open_ == false)
		{
			//DebugMessage("File is not opened");
			return ULL_MAX;
		}
		NTSTATUS status;
		LARGE_INTEGER li_file_size = { 0 };
		if (NT_SUCCESS((status = FsRtlGetFileSize(p_file_object_, &li_file_size))))
		{
			return li_file_size.QuadPart;
		}
		return ULL_MAX;
	}

	void FileFlt::Close()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return;
		}
		if (p_file_object_ != nullptr && pre_alloc_file_object_ == false)
		{
			ObDereferenceObject(p_file_object_);
			p_file_object_ = nullptr;
		}
		if (file_handle_ != nullptr)
		{
			FltClose(file_handle_);
			file_handle_ = nullptr;
		}
	}

	FileFlt::~FileFlt()
	{
		Close();
	}

	bool ZwIsFileExist(const String<WCHAR>& file_path_str)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}

		HANDLE file_handle = nullptr;
		UNICODE_STRING uni_str = { file_path_str.Size() * sizeof(WCHAR), file_path_str.Size() * sizeof(WCHAR), (PWCH)file_path_str.Data() };

		OBJECT_ATTRIBUTES obj_attr;
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		IO_STATUS_BLOCK io_status_block = { 0 };

		NTSTATUS status = ZwCreateFile(&file_handle, FILE_READ_ATTRIBUTES, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);

		if (NT_SUCCESS(status))
		{
			ZwClose(file_handle);
			return true;
		}
		else if (status == STATUS_SHARING_VIOLATION)
		{
			return true;
		}

		return false;
	}

	ull IoGetFileSize(const WCHAR* file_path)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return 0;
		}
		UNICODE_STRING uni_str;
		OBJECT_ATTRIBUTES obj;
		HANDLE h_file;
		NTSTATUS status;
		IO_STATUS_BLOCK io_status;
		FILE_STANDARD_INFORMATION file_std_info;

		RtlInitUnicodeString(&uni_str, file_path);
		InitializeObjectAttributes(&obj, &uni_str, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
		status = IoCreateFile(&h_file, FILE_READ_ATTRIBUTES, &obj, &io_status, 0, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0, CreateFileTypeNone, NULL, IO_NO_PARAMETER_CHECKING);
		if (!NT_SUCCESS(status))
		{
			DebugMessage("IoCreateFile failed: %x", status);
			return NULL;
		}
		status = ZwQueryInformationFile(h_file, &io_status, &file_std_info, sizeof(file_std_info), FileStandardInformation);
		if (!NT_SUCCESS(status))
		{
			DebugMessage("ZwQueryInformationFile failed: %x", status);
			ZwClose(h_file);
			return NULL;
		}
		ZwClose(h_file);
		return file_std_info.EndOfFile.QuadPart;
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

	String<WCHAR> NormalizeDevicePathStr(const String<WCHAR>& path)
	{
		UNICODE_STRING path_uni_str = { path.Size() * sizeof(WCHAR), path.Size() * sizeof(WCHAR) , (PWCH)path.Data() };
		String<WCHAR> normalized;
		normalized.Resize(path.Size() * 2);
		UNICODE_STRING normalized_uni_str = { normalized.Size() * sizeof(WCHAR), normalized.Size() * sizeof(WCHAR) , (PWCH)normalized.Data() };
		if (NT_SUCCESS(NormalizeDevicePath(&path_uni_str, &normalized_uni_str)))
		{
			normalized.Resize(wcsnlen(normalized.Data(), normalized.Size()));
		}
		else
		{
			normalized.Clear();
		}
		return normalized;
	}

}