#include "file_type_iden.h"
#include "ulti/support.h"
#include "ulti/debug.h"
#include "../file_type/txt.h"
#include "../file_type/compress.h"
#include "../file_type/image.h"
#include "../file_type/pdf.h"
#include "../file_type/av.h"
//#include "../file_type/ole.h"

namespace type_iden
{

	FileType* s_instance = nullptr;
	std::mutex s_mutex;

	FileType* FileType::GetInstance()
	{
		if (!s_instance)
		{
			std::lock_guard<std::mutex> lock(s_mutex);
			if (!s_instance)   // double-checked locking
			{
				s_instance = new FileType();
			}
		}
		return s_instance;
	}

	void FileType::DeleteInstance()
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		if (s_instance)
		{
			delete s_instance;
			s_instance = nullptr;
		}
	}

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
#ifdef _M_IX86
		if (trid_ != nullptr)
		{
			delete trid_;
			trid_ = nullptr;
		}
#endif // _M_IX86
	}

	bool FileType::Init() {
#ifdef _M_IX86
		if (ulti::CreateDir(TEMP_DIR) == false)
		{
			PrintDebugW(L"Create temp dir %ws failed", TEMP_DIR);
			return false;
		}
		current_path = ...;
		std::wstring trid_dir = current_path + L"TrID";
		if (kFileType->InitTrid(trid_dir, trid_dir + L"TrIDLib.dll") == false)
		{
			PrintDebugW(L"TrID init failed");
			return false;
		}
#endif // _M_IX86
		return true;
	}

	void FileType::Uninit() {
#ifdef _M_IX86
		if (trid_ != nullptr)
		{
			delete trid_;
			trid_ = nullptr;
		}
#endif // _M_IX86
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

	vector<string> FileType::GetTypes(const wstring& file_path, DWORD* p_status, ull* p_file_size)
	{
		vector<string> types;
		if (p_status == nullptr)
		{
			return types;
		}

		*p_status = ERROR_SUCCESS;

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

		*p_file_size = static_cast<size_t>(li_size.QuadPart);
		if (*p_file_size > FILE_MAX_SIZE_SCAN) {
			*p_status = ERROR_FILE_TOO_LARGE;
			return types;
		}

		UCHAR* data = (UCHAR*)HeapAlloc(GetProcessHeap(), 0, *p_file_size);
		if (!data) {
			*p_status = ERROR_OUTOFMEMORY;
			return types;
		}
		defer{ HeapFree(GetProcessHeap(), 0, data); };

		DWORD bytes_read = 0;
		BOOL ok = ReadFile(file_handle, data, *p_file_size, &bytes_read, nullptr);
		if (!ok || bytes_read != *p_file_size) {
			*p_status = GetLastError();
			return types;
		}

		const span<UCHAR> span_data(data, *p_file_size);

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
		TryGetTypes(GetAudioVideoTypes);
		TryGetTypes(Get7zTypes);
		TryGetTypes(GetTxtTypes);
		TryGetTypes(GetZlibTypes);
		TryGetTypes(GetGzipTypes);

		//TryGetTypes(GetWebpTypes); // Bad performance, do not use.
		//TryGetTypes(GetOleTypes); // Bad performance, do not implement.
		return types;
	}
}
