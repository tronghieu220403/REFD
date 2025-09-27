#include "file_type_iden.h"
#include "ulti/support.h"
#include "ulti/debug.h"
#include "file_manager.h"
#include "../file_type/txt.h"
#include "../file_type/zip.h"

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
		trid_ = new type_iden::TrID();
		wstring trid_dir = wstring(PRODUCT_PATH) + L"TrID";
		if (trid_ == nullptr || trid_->Init(trid_dir, trid_dir + L"TrIDLib.dll") == false)
		{
			PrintDebugW(L"TrID init failed");
			return false;
		}

		return true;
	}

	vector<string> FileType::GetTypes(const wstring& file_path)
	{
		vector<string> types;

		types.emplace_back(trid_->GetTypes(file_path));

		HANDLE file_handle = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) {
			return types;
		}
		defer{ CloseHandle(file_handle); };

		LARGE_INTEGER li_size{};
		if (!GetFileSizeEx(file_handle, &li_size)) {
			return types;
		}

		size_t file_size = static_cast<size_t>(li_size.QuadPart);
		if (file_size < FILE_MIN_SIZE_SCAN || file_size > FILE_MAX_SIZE_SCAN) {
			return types;
		}

		HANDLE mapping_handle = CreateFileMappingW(file_handle, nullptr,
			PAGE_READONLY, 0, li_size.LowPart, nullptr);
		if (!mapping_handle) {
			return types;
		}
		defer{ CloseHandle(mapping_handle); };

		UCHAR* data = (UCHAR *)MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, li_size.LowPart);
		if (!data) {
			return types;
		}
		defer{ UnmapViewOfFile(data); };

		const span<UCHAR> span_data(data, file_size);

		types.emplace_back(GetTxtTypes(span_data));

		return types;
	}
}
