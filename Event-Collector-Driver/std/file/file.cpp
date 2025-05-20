#include "file.h"
#include <Fltkernel.h>

namespace file
{
	ull IoGetFileSize(const WCHAR* file_path)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return 0;
		}
		UNICODE_STRING uni_str;
		OBJECT_ATTRIBUTES Obj;
		HANDLE hFile;
		NTSTATUS status;
		IO_STATUS_BLOCK io_status;
		FILE_STANDARD_INFORMATION file_std_info;

		RtlInitUnicodeString(&uni_str, file_path);
		InitializeObjectAttributes(&Obj, &uni_str, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
		status = IoCreateFile(&hFile, FILE_READ_ATTRIBUTES, &Obj, &io_status, 0, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0, CreateFileTypeNone, NULL, IO_NO_PARAMETER_CHECKING);
		if (!NT_SUCCESS(status))
		{
			DebugMessage("IoCreateFile failed: %x", status);
			return NULL;
		}
		status = ZwQueryInformationFile(hFile, &io_status, &file_std_info, sizeof(file_std_info), FileStandardInformation);
		if (!NT_SUCCESS(status))
		{
			DebugMessage("ZwQueryInformationFile failed: %x", status);
			ZwClose(hFile);
			return NULL;
		}
		ZwClose(hFile);
		return file_std_info.EndOfFile.QuadPart;

	}

	File::File(const String<WCHAR>& current_path) :
		file_path_(current_path)
	{

	}

	bool File::Open(const WCHAR* file_path)
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

		NTSTATUS status = ZwCreateFile(&file_handle_, FILE_GENERIC_READ | FILE_GENERIC_WRITE, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
		return NT_SUCCESS(status);
	}

	ull File::Read(PVOID buffer, ull length)
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

	ull File::Append(PVOID buffer, ull length)
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return false;
		}
		IO_STATUS_BLOCK io_status_block;
		if (!NT_SUCCESS(ZwWriteFile(file_handle_, NULL, NULL, NULL, &io_status_block, buffer, (ULONG)length, NULL, NULL)))
		{
			return 0;
		}
		return io_status_block.Information;
	}

	ull File::Size()
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


	void File::Close() {
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			return;
		}
		ZwClose(file_handle_);
		file_handle_ = nullptr;
	}

	File::~File()
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

	bool FileFlt::Open()
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
		UNICODE_STRING uni_str;
		OBJECT_ATTRIBUTES obj_attr;
		IO_STATUS_BLOCK io_status_block;
		RtlInitUnicodeString(&uni_str, file_path_.Data());
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		NTSTATUS status = FltCreateFileEx(p_filter_handle_, p_instance_, &file_handle_, &p_file_object_, FILE_GENERIC_READ | FILE_GENERIC_WRITE, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, create_disposition_, FILE_NON_DIRECTORY_FILE, NULL, 0, 0);
		if (NT_SUCCESS(status))
		{
			is_open_ = true;
		}
		else
		{
			DebugMessage("FltCreateFile %ws failed: %x", file_path_.Data(), status);
			is_open_ = false;
			file_handle_ = nullptr;
			p_file_object_ = nullptr;
		}
		return NT_SUCCESS(status);
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
        DebugMessage("ReadWithOffset %ws, offset: %llu, length: %llu", file_path_.Data(), offset, length);
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
				DebugMessage("FltReadFile read %u bytes, exptected %llu bytes", bytes_read, new_length);
			}
			else
			{
				DebugMessage("FltReadFile failed: %x", status);
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
			DebugMessage("FltWriteFile failed: %x", status);
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
		UNICODE_STRING uni_str;
		OBJECT_ATTRIBUTES obj_attr;
		IO_STATUS_BLOCK io_status_block;
		RtlInitUnicodeString(&uni_str, file_path_.Data());
		InitializeObjectAttributes(&obj_attr, &uni_str, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		NTSTATUS status = FltCreateFile(p_filter_handle_, p_instance_, &file_handle_tmp, FILE_READ_ATTRIBUTES, &obj_attr, &io_status_block, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0, 0);
		if (NT_SUCCESS(status))
		{
			FltClose(file_handle_tmp);
			return true;
		}
		if (status == STATUS_OBJECT_NAME_NOT_FOUND
			|| io_status_block.Status == FILE_DOES_NOT_EXIST
			|| status == STATUS_NO_SUCH_FILE
			|| status == STATUS_OBJECT_PATH_NOT_FOUND
			|| status == STATUS_OBJECT_NAME_INVALID
			|| status == STATUS_OBJECT_PATH_INVALID
			)
		{
			return false;
		}
		return true;
	}

	ull FileFlt::Size()
	{
		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			DebugMessage("KeGetCurrentIrql != PASSIVE_LEVEL");
			return ULL_MAX;
		}
		if (is_open_ == false)
		{
			DebugMessage("File is not opened");
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

}