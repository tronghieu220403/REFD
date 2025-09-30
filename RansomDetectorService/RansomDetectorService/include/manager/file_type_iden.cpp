#include "file_type_iden.h"
#include "ulti/support.h"
#include "ulti/debug.h"
#include "file_manager.h"
#include "../file_type/txt.h"
#include "../file_type/zip.h"
#include "../file_type/png.h"
#include "../file_type/jpg.h"
#include "../file_type/7z.h"
#include "../file_type/pdf.h"
#include "../file_type/rar.h"
#include "../file_type/gzip.h"
//#include "../file_type/ole.h"

namespace type_iden
{
	// Checks whether two vectors of strings have any common element.
	// Returns true if there is at least one string that appears in both vectors.
	bool HasCommonType(const vector<string>& types1, const vector<string>& types2) {
		// To optimize, insert the smaller vector into a hash set for faster lookup.
		const vector<string>& smaller = (types1.size() < types2.size()) ? types1 : types2;
		const vector<string>& larger = (types1.size() < types2.size()) ? types2 : types1;

		// Create a set from the smaller vector for O(1) lookups
		unordered_set<string> type_set(smaller.begin(), smaller.end());

		// Check if any element in the larger vector exists in the set
		for (const auto& type : larger) {
			if (type_set.count(type) > 0) {
				return true;
			}
		}

		// No common elements found
		return false;
	}

	wstring CovertTypesToString(const vector<string>& types)
	{
		string types_str = "<";
		for (const auto& type : types)
		{
			types_str += "\"" + type + "\", ";
		}
        types_str += ">";
        return ulti::StrToWstr(types_str);
	}

	FileType::~FileType()
	{
		if (trid_ != nullptr)
		{
			delete trid_;
			trid_ = nullptr;
		}
	}

	bool FileType::InitTrid(const wstring& defs_dir, const wstring& trid_dll_path)
	{
#ifdef _M_IX86
		trid_ = new type_iden::TrID();
		if (trid_ == nullptr || trid_->Init(defs_dir, trid_dll_path) == false)
		{
			PrintDebugW(L"TrID init failed");
			return false;
		}
#endif // _M_IX86

		return true;
	}

	vector<string> FileType::GetTypes(const wstring& file_path, DWORD* p_status)
	{
		vector<string> types;
		if (p_status == nullptr)
		{
			return types;
		}

		*p_status = ERROR_SUCCESS;
		size_t file_size = 0;

#ifdef _M_IX86
		defer
		{
			if (types.size() == 0 && file_size > 0 && file_size <= FILE_MAX_SIZE_SCAN)
			{
				ulti::AddVectorsInPlace(types, trid_->GetTypes(file_path));
			}
		};
#endif // _M_IX86 

		HANDLE file_handle = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) {
			*p_status = GetLastError();
			return types;
		}
		defer{ CloseHandle(file_handle); };

		LARGE_INTEGER li_size{};
		if (!GetFileSizeEx(file_handle, &li_size)) {
			*p_status = GetLastError();
			return types;
		}

		file_size = static_cast<size_t>(li_size.QuadPart);
		if (file_size < FILE_MIN_SIZE_SCAN || file_size > FILE_MAX_SIZE_SCAN) {
			*p_status = ERROR_FILE_TOO_LARGE;
			return types;
		}

		HANDLE mapping_handle = CreateFileMappingW(file_handle, nullptr,
			PAGE_READONLY, 0, li_size.LowPart, nullptr);
		if (!mapping_handle) {
			*p_status = GetLastError();
			return types;
		}
		defer{ CloseHandle(mapping_handle); };

		UCHAR* data = (UCHAR *)MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, li_size.LowPart);
		if (!data) {
			*p_status = GetLastError();
			return types;
		}
		defer{ UnmapViewOfFile(data); };

		const span<UCHAR> span_data(data, file_size);

		auto TryGetTypes = [&](auto&& fn) -> void {
			if (types.size() > 0) return; 
			auto new_type = fn(span_data);
			if (!new_type.empty()) {
				ulti::AddVectorsInPlace(types, new_type);
			}
		};

		TryGetTypes(GetPdfTypes);
		TryGetTypes(GetZipTypes);
		TryGetTypes(GetRarTypes);
		TryGetTypes(GetPngTypes);
		TryGetTypes(GetJpgTypes);
		TryGetTypes(Get7zTypes);
		TryGetTypes(GetTxtTypes);
		TryGetTypes(GetGzipTypes);

		//TryGetTypes(GetOleTypes); // Bad performance, do not implement.
		return types;
	}
}
