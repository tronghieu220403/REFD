#pragma once

#include "flt-ex.h"

namespace flt
{
	bool IsTrustedRequestor(PFLT_CALLBACK_DATA data)
	{
		if (data->RequestorMode == KernelMode)
		{
			return true;
		}
		return false;
	}

	std::WString DebugIrpFlags(ULONG flag)
	{
		using namespace std;
		WString str;
		if (FlagOn(flag, IRP_NOCACHE))
		{
			str += WString(L"IRP_NOCACHE");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_PAGING_IO))
		{
			str += WString(L"IRP_PAGING_IO");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_MOUNT_COMPLETION))
		{
			str += WString(L"IRP_MOUNT_COMPLETION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_SYNCHRONOUS_API))
		{
			str += WString(L"IRP_SYNCHRONOUS_API");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_ASSOCIATED_IRP))
		{
			str += WString(L"IRP_ASSOCIATED_IRP");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_BUFFERED_IO))
		{
			str += WString(L"IRP_BUFFERED_IO");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_DEALLOCATE_BUFFER))
		{
			str += WString(L"IRP_DEALLOCATE_BUFFER");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_INPUT_OPERATION))
		{
			str += WString(L"IRP_INPUT_OPERATION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_SYNCHRONOUS_PAGING_IO))
		{
			str += WString(L"IRP_SYNCHRONOUS_PAGING_IO");
			str += WString(L", ");
		}

		if (FlagOn(flag, IRP_CREATE_OPERATION))
		{
			str += WString(L"IRP_CREATE_OPERATION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_READ_OPERATION))
		{
			str += WString(L"IRP_READ_OPERATION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_WRITE_OPERATION))
		{
			str += WString(L"IRP_WRITE_OPERATION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_CLOSE_OPERATION))
		{
			str += WString(L"IRP_CLOSE_OPERATION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_DEFER_IO_COMPLETION))
		{
			str += WString(L"IRP_DEFER_IO_COMPLETION");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_OB_QUERY_NAME))
		{
			str += WString(L"IRP_OB_QUERY_NAME");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_HOLD_DEVICE_QUEUE))
		{
			str += WString(L"IRP_HOLD_DEVICE_QUEUE");
			str += WString(L", ");
		}
		if (FlagOn(flag, IRP_UM_DRIVER_INITIATED_IO))
		{
			str += WString(L"IRP_UM_DRIVER_INITIATED_IO");
			str += WString(L", ");
		}

		return str;
	}

	std::WString GetFileFullPathName(PFLT_CALLBACK_DATA data)
	{
		if (data == nullptr)
		{
			return std::WString();
		}
		std::WString res;
		PFLT_FILE_NAME_INFORMATION file_name_info = nullptr;
		NTSTATUS status = FltGetFileNameInformation(data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP, &file_name_info);
		if (status == STATUS_SUCCESS)
		{
			res = std::WString(file_name_info->Name);
			FltReleaseFileNameInformation(file_name_info);
		}
		else if (status == STATUS_FLT_NAME_CACHE_MISS && FltGetFileNameInformation(data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY, &file_name_info) == STATUS_SUCCESS)
		{
			res = std::WString(file_name_info->Name);
			FltReleaseFileNameInformation(file_name_info);
		}
		return res;
	}

	FileInfoShort::FileInfoShort(PUCHAR base_va, ull next_entry_offset_rva, ull file_name_rva, ull file_name_length_rva, ull file_attributes_rva)
		:file_info_addr_(base_va), next_entry_offset_rva_   (next_entry_offset_rva), file_name_rva_(file_name_rva), file_name_length_rva_(file_name_length_rva), file_attributes_rva_(file_attributes_rva)
	{

	}

	FileInfoShort::FileInfoShort(PUCHAR base_va, ull next_entry_offset_rva, ull file_name_rva, ull file_name_length_rva)
		: file_info_addr_(base_va), next_entry_offset_rva_(next_entry_offset_rva), file_name_rva_(file_name_rva), file_name_length_rva_(file_name_length_rva)
	{

	}


	FileInfoShort::FileInfoShort(const FileInfoShort* file_info, const PUCHAR file_info_addr)
	{
		file_info_addr_ = file_info_addr;
		next_entry_offset_rva_ = file_info->next_entry_offset_rva_;
		file_name_rva_ = file_info->file_name_rva_;
		file_name_length_rva_ = file_info->file_name_length_rva_;
		file_attributes_rva_ = file_info->file_attributes_rva_;
	}

	ULONG FileInfoShort::GetNextEntryOffset() const
	{
		return *(ULONG*)((PUCHAR)file_info_addr_ + next_entry_offset_rva_);
	}

	PWCHAR FileInfoShort::GetFileName() const
	{
		return (PWCHAR)((PUCHAR)file_info_addr_ + file_name_rva_);
	}

	ULONG FileInfoShort::GetFileNameLength() const
	{
		return *(ULONG*)((PUCHAR)file_info_addr_ + file_name_length_rva_);
	}

	ULONG FileInfoShort::GetFileAttributes() const
	{
		if (file_attributes_rva_ != ULL_MAX)
		{
			return *(ULONG*)((PUCHAR)file_info_addr_ + file_attributes_rva_);
		}
		return ULONG();
	}

	PUCHAR FileInfoShort::GetBaseAddr() const
	{
		return file_info_addr_;
	}

	ULONG FileInfoShort::Length() const
	{
		return (ULONG)file_name_rva_ + GetFileNameLength();
	}

	PUCHAR FileInfoShort::GetNextEntryAddr() const
	{
		return GetBaseAddr() + GetNextEntryOffset();
	}

	void FileInfoShort::SetNextEntryOffset(const ULONG next_entry_val)
	{
		*(ULONG*)((PUCHAR)file_info_addr_ + next_entry_offset_rva_) = next_entry_val;
	}

	void FileInfoShort::SetFileName(PWCHAR current_path)
	{
		*(PWCHAR*)((PUCHAR)file_info_addr_ + file_name_rva_) = current_path;
	}

	void FileInfoShort::SetFileNameLength(ULONG length)
	{
		*(ULONG*)((PUCHAR)file_info_addr_ + file_name_length_rva_) = length;
	}

	void FileInfoShort::SetFileAttributes(ULONG file_attributes)
	{
		if (file_attributes_rva_ != ULL_MAX)
		{
			*(ULONG*)((PUCHAR)file_info_addr_ + file_attributes_rva_) = file_attributes;
		}
	}

	void FileInfoShort::SetBaseAddr(const PUCHAR file_info_addr)
	{
		file_info_addr_ = file_info_addr;
	}

	bool FileInfoShort::IsNull() const
	{
		if (file_info_addr_ == nullptr || (next_entry_offset_rva_ == file_name_rva_ || file_name_rva_ == file_name_length_rva_ || file_name_length_rva_ == next_entry_offset_rva_))
		{
			return true;
		}
		return false;
	}
}