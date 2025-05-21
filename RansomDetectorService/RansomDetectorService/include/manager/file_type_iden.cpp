#include "file_type_iden.h"
#include "ulti/support.h"
#include "ulti/debug.h"
#include "file_manager.h"

namespace type_iden
{
	// Checks whether two vectors of strings have any common element.
	// Returns true if there is at least one string that appears in both vectors.
	bool HasCommonType(const std::vector<std::string>& types1, const std::vector<std::string>& types2) {
		// To optimize, insert the smaller vector into a hash set for faster lookup.
		const std::vector<std::string>& smaller = (types1.size() < types2.size()) ? types1 : types2;
		const std::vector<std::string>& larger = (types1.size() < types2.size()) ? types2 : types1;

		// Create a set from the smaller vector for O(1) lookups
		std::unordered_set<std::string> type_set(smaller.begin(), smaller.end());

		// Check if any element in the larger vector exists in the set
		for (const auto& type : larger) {
			if (type_set.count(type) > 0) {
				return true;
			}
		}

		// No common elements found
		return false;
	}

	bool CheckPrintableUTF16(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = buffer.size() / sizeof(wchar_t);

		for (size_t i = 0; i < buffer.size(); i += 2)
		{
			wchar_t c = *(wchar_t*)&buffer[i];
			if (iswprint(c) || iswspace(c)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF8(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = 0;
		size_t i = 0;
		while (i < buffer.size()) {

			unsigned char c = buffer[i];
			if (c < 0x80) { // 1-byte ASCII (7-bit)
				total_chars++;
				if (isprint(c) || isspace(c)) {
					printable_chars++;
				}
				i++;
			}
			else if ((c & 0xE0) == 0xC0) { // 2-byte UTF-8
				if (i + 1 < buffer.size()) {
					wchar_t wchar = ((c & 0x1F) << 6) | (buffer[i + 1] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 2;
				/*
				// Should be like this:
				if (i + 1 < buffer.size()) {
					if (buffer.size() == BEGIN_WIDTH + BEGIN_WIDTH && i < BEGIN_WIDTH && i + 1 >= BEGIN_WIDTH) // Char between the merge may not correct
					{
						i = BEGIN_WIDTH;
						continue;
					}
					wchar_t wchar = ((c & 0x1F) << 6) | (buffer[i + 1] & 0x3F);
					if (iswprint(wchar) || iswspace(wchar)) {
						total_chars += 2;
						printable_chars += 2;
						i += 2;
					}
					else
					{
						if (buffer.size() == BEGIN_WIDTH + BEGIN_WIDTH && i >= BEGIN_WIDTH && i <= BEGIN_WIDTH + 2) // May be there is a first char in UTF-8 char disappeared.
						{
							i += 1;
						}
						else
						{
							total_chars += 2;
							i += 2;
						}
					}
				}
				*/
			}
			else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8
				if (i + 2 < buffer.size()) {
					wchar_t wchar = ((c & 0x0F) << 12) | ((buffer[i + 1] & 0x3F) << 6) | (buffer[i + 2] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 3;
			}
			else if ((c & 0xF8) == 0xF0) { // 4-byte UTF-8
				if (i + 3 < buffer.size()) {
					wchar_t wchar = ((c & 0x07) << 18) | ((buffer[i + 1] & 0x3F) << 12) | ((buffer[i + 2] & 0x3F) << 6) | (buffer[i + 3] & 0x3F);
					total_chars++;
					if (iswprint(wchar) || iswspace(wchar)) {
						printable_chars++;
					}
				}
				i += 4;
			}
			else {
				i++; // Skip invalid bytes
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableANSI(const std::vector<unsigned char>& buffer)
	{
		if (buffer.size() == 0) {
			return true;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = 0;

		for (unsigned char c : buffer) {
			total_chars++;
			if (isprint(c) || isspace(c)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool IsPrintableFile(const fs::path& file_path)
	{
		try
		{
			auto file_size = manager::GetFileSize(file_path);
			if (file_size == 0 || file_size > FILE_MAX_SIZE_SCAN) {
				return false;
			}
			std::ifstream file(file_path, std::ios::binary); // Open file in binary mode
			if (!file.is_open()) {
				return false; // File cannot be opened
			}

			std::vector<unsigned char> buffer(file_size); // Buffer to hold file data
			file.read(reinterpret_cast<char*>(buffer.data()), file_size); // Read all data into buffer
			file.close();

			// Check against different encodings and return true if any matches
			if (CheckPrintableUTF8(buffer) || CheckPrintableUTF16(buffer) || CheckPrintableANSI(buffer)) {
				return true;
			}
		}
		catch (...)
		{

		}
		return false; // If no encoding matches, return false
	}

	TrID::~TrID()
	{
		if (trid_api != nullptr)
		{
			delete trid_api;
			trid_api = nullptr;
		}
		issue_thread_id = 0;
	}

	bool TrID::Init(const std::wstring& defs_dir, const std::wstring& trid_dll_path)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (ulti::IsCurrentX86Process() == false)
		{
			PrintDebugW(L"Current process is not a x86 process");
			return false;
		}
		if (trid_api != nullptr)
		{
			delete trid_api;
			trid_api = nullptr;
		}
		try
		{
			trid_api = new TridApi(ulti::WstrToStr(defs_dir).c_str(), trid_dll_path.c_str());
		}
		catch (int trid_error_code)
		{
			if (trid_error_code == TRID_MISSING_LIBRARY)
			{
				PrintDebugW(L"TrIDLib.dll not found");
			}
			if (trid_api != nullptr)
			{
				delete trid_api;
				trid_api = nullptr;
			}
			return false;
		}
		issue_thread_id = GetCurrentThreadId();
		return true;
	}

	std::vector<std::string> TrID::GetTypes(const fs::path& file_path)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		PrintDebugW(L"Getting types of file: %s", file_path.c_str());
		/*
		if (issue_thread_id != GetCurrentThreadId())
		{
			PrintDebugW(L"TrID API is not thread-safe");
			return {};
		}
		*/
		if (trid_api == nullptr)
		{
			PrintDebugW(L"TrID API is not initialized");
			return {};
		}
		if (file_path.empty())
		{
			PrintDebugW(L"File path is empty");
			return {};
		}
		int ret;
		std::vector<std::string> types;

		trid_api->SubmitFileA(file_path.string().c_str());
		ret = trid_api->Analyze();
		if (ret)
		{
			char buf[512];
			*buf = 0;
			ret = trid_api->GetInfo(TRID_GET_RES_NUM, 0, buf);
			for (int i = ret + 1; i--;)
			{
				trid_api->GetInfo(TRID_GET_RES_FILETYPE, i, buf);
				std::string type_str(buf);
				if (type_str.find("ransom") != std::string::npos || type_str.find("ncrypt") != std::string::npos)
				{
					continue;
				}
				trid_api->GetInfo(TRID_GET_RES_FILEEXT, i, buf);
				std::string ext_str(buf);

				if (ext_str.size() > 0)
				{
					std::stringstream ss(ext_str);
					std::string ext;
					while (std::getline(ss, ext, '/'))
					{
						ulti::ToLowerOverride(ext);
						types.push_back(ext); // Save found extension
					}
				}
				else
				{
					if (type_str.size() > 0)
					{
						ulti::ToLowerOverride(type_str);
						types.push_back(type_str);
					}
				}
				*buf = 0;
			}
		}

		// Read all bytes of the file
		std::ifstream file(file_path, std::ios::binary); // Open file in binary mode
		if (file.is_open()) {
			if (IsPrintableFile(file_path))
			{
				PrintDebugW(L"File %ws is a printable file", file_path.c_str());
				types.push_back("txt");
			}
		}
		else
		{
			PrintDebugW(L"File %ws cannot be opened", file_path.c_str());
		}

		if (types.size() == 0)
		{
			types.push_back("");
		}
#ifdef _DEBUG
		std::string types_str = "<";
		for (const auto& type : types)
		{
			types_str += "\"" + type + "\", ";
		}
		types_str[types_str.size() - 2] = '>';
		PrintDebugW(L"File types: %ws", ulti::StrToWstr(types_str).c_str());
#endif // _DEBUG

		return types;
	}

	std::wstring CovertTypesToString(const std::vector<std::string>& types)
	{
		std::string types_str = "<";
		for (const auto& type : types)
		{
			types_str += "\"" + type + "\", ";
		}
        types_str += ">";
        return ulti::StrToWstr(types_str);
	}
}
