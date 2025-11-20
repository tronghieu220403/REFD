#pragma once


#include "../std/vector/vector.h"
#include "../std/string/wstring.h"
#include "../std/sync/mutex.h"
#include "../std/map/map.h"
#include "../std/memory/pair.h"
#include "../template/debug.h"

#include <wdm.h>
#include <Ntstrsafe.h>
#include <fltKernel.h>

#define offsetof(st, m) ((ull)(&((st *)0)->m))

namespace flt
{
	bool IsTrustedRequestor(PFLT_CALLBACK_DATA data);

	std::WString DebugIrpFlags(ULONG flag);

	std::WString GetFileFullPathName(PFLT_CALLBACK_DATA data);

	class FileInfoShort
	{
	private:
		PUCHAR file_info_addr_ = nullptr;
		ull next_entry_offset_rva_ = 0;
		ull file_name_rva_ = 0;
		ull file_name_length_rva_ = 0;
		ull file_attributes_rva_ = ULL_MAX;
	public:

		FileInfoShort() = default;

		FileInfoShort(PUCHAR base_va, ull next_entry_offset_rva, ull file_name_rva, ull file_name_length_rva);

		FileInfoShort(PUCHAR base_va, ull next_entry_offset_rva, ull file_name_rva, ull file_name_length_rva, ull file_attributes_rva);

		FileInfoShort(const FileInfoShort*, const PUCHAR);

		ULONG GetNextEntryOffset() const;
		PWCHAR GetFileName() const;
		ULONG GetFileNameLength() const;
		ULONG GetFileAttributes() const;
		PUCHAR GetBaseAddr() const;

		ULONG Length() const;
		PUCHAR GetNextEntryAddr() const;

		void SetNextEntryOffset(const ULONG);
		void SetFileName(const PWCHAR);
		void SetFileNameLength(const ULONG);
		void SetFileAttributes(const ULONG);
		void SetBaseAddr(const PUCHAR);

		bool IsNull() const;
	};

}