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


	bool IsPrintableCodepoint(uint32_t cp) {
		if (cp == '\n' || cp == '\r' || cp == '\t') return true;
		if (cp >= 0x20 && cp < 0x7F) return true; // ASCII printable
		return iswprint((wint_t)cp) || iswspace((wint_t)cp);
	}

	bool CheckPrintableUTF16(const std::span<unsigned char>& buffer)
	{
		size_t i = 0;
		bool little_endian = true;

		// BOM check
		if (buffer[0] == 0xFF && buffer[1] == 0xFE) {
			little_endian = true;
			i = 2;
		}
		else if (buffer[0] == 0xFE && buffer[1] == 0xFF) {
			little_endian = false;
			i = 2;
		}

		std::streamsize printable_chars = 0;
		std::streamsize total_chars = buffer.size() / sizeof(wchar_t);

		auto read_u16 = [&](size_t idx) -> uint16_t {
			if (little_endian)
				return buffer[idx] | (buffer[idx + 1] << 8);
			else
				return (buffer[idx] << 8) | buffer[idx + 1];
			};

		while (i + 1 < buffer.size()) {
			uint16_t w1 = read_u16(i);
			i += 2;

			uint32_t codepoint = 0;

			if (w1 >= 0xD800 && w1 <= 0xDBFF) {
				// high surrogate
				if (i + 1 < buffer.size()) {
					uint16_t w2 = read_u16(i);
					if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
						codepoint = 0x10000 + (((w1 - 0xD800) << 10) | (w2 - 0xDC00));
						i += 2;
					}
					else {
						// invalid surrogate
						continue;
					}
				}
			}
			else {
				codepoint = w1;
			}

			total_chars++;
			if (IsPrintableCodepoint(codepoint)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) {
			return false;
		}

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF8(const std::span<unsigned char>& buffer)
	{
		std::streamsize printable_chars = 0;
		std::streamsize total_chars = 0;
		size_t i = 0;
		if (buffer[0] == 0xef && buffer[1] == 0xbb && buffer[2] == 0xbf)
		{
			i = 3; // skip UTF-8 BOM
		}
		while (i < buffer.size()) {
			unsigned char c = buffer[i];
			uint32_t codepoint = 0;
			size_t seq_len = 0;

			if (c < 0b10000000) { // 1-byte ASCII: 0xxxxxxx
				codepoint = c;
				seq_len = 1;
			}
			else if (i + 1 < buffer.size()
				&& (c & 0b11100000) == 0b11000000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000)
				// 2-byte UTF-8: 110xxxxx 10xxxxxx
			{
				codepoint = ((c & 0b00011111) << 6) | (buffer[i + 1] & 0b00111111);
				seq_len = 2;
			}
			else if (i + 2 < buffer.size()
				&& (c & 0b11110000) == 0b11100000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000
				&& (buffer[i + 2] & 0b11000000) == 0b10000000)
				// 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx
			{
				codepoint = ((c & 0x0F) << 12) | ((buffer[i + 1] & 0b00111111) << 6) | (buffer[i + 2] & 0b00111111);
				seq_len = 3;
			}
			else if (i + 3 < buffer.size()
				&& (c & 0b11111000) == 0b11110000
				&& (buffer[i + 1] & 0b11000000) == 0b10000000
				&& (buffer[i + 2] & 0b11000000) == 0b10000000
				&& (buffer[i + 3] & 0b11000000) == 0b10000000)
				// 4-byte UTF-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
			{
				codepoint = ((c & 0b00000111) << 18) | ((buffer[i + 1] & 0b00111111) << 12)
					| ((buffer[i + 2] & 0b00111111) << 6) | (buffer[i + 3] & 0b00111111);
				seq_len = 4;
			}
			else {
				// invalid byte
				i++;
				continue;
			}

			total_chars++;
			if (IsPrintableCodepoint(codepoint)) {
				printable_chars++;
			}

			i += seq_len;
		}

		if (total_chars == 0) return false;
		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool CheckPrintableUTF32(const std::span<unsigned char>& buffer)
	{
		size_t i = 0;
		bool little_endian = true;

		// BOM check
		if (buffer.size() >= 4) {
			if (buffer[0] == 0xFF && buffer[1] == 0xFE &&
				buffer[2] == 0x00 && buffer[3] == 0x00) {
				little_endian = true;
				i = 4;
			}
			else if (buffer[0] == 0x00 && buffer[1] == 0x00 &&
				buffer[2] == 0xFE && buffer[3] == 0xFF) {
				little_endian = false;
				i = 4;
			}
		}

		auto read_u32 = [&](size_t idx) -> uint32_t {
			if (little_endian) {
				return (uint32_t)buffer[idx] |
					((uint32_t)buffer[idx + 1] << 8) |
					((uint32_t)buffer[idx + 2] << 16) |
					((uint32_t)buffer[idx + 3] << 24);
			}
			else {
				return ((uint32_t)buffer[idx] << 24) |
					((uint32_t)buffer[idx + 1] << 16) |
					((uint32_t)buffer[idx + 2] << 8) |
					(uint32_t)buffer[idx + 3];
			}
			};

		std::streamsize total_chars = 0;
		std::streamsize printable_chars = 0;

		while (i + 3 < buffer.size()) {
			uint32_t cp = read_u32(i);
			i += 4;

			// UTF-32 validity 
			if (cp > 0x10FFFF) continue;
			if (cp >= 0xD800 && cp <= 0xDFFF) continue; // invalid surrogate

			total_chars++;
			if (IsPrintableCodepoint(cp)) {
				printable_chars++;
			}
		}

		if (total_chars == 0) return false;

		return !BelowTextThreshold(printable_chars, total_chars);
	}

	bool IsPrintableFile(const std::wstring& file_path)
	{
		HANDLE file_handle = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) {
			return false;
		}
		defer{ CloseHandle(file_handle); };

		LARGE_INTEGER li_size{};
		if (!GetFileSizeEx(file_handle, &li_size)) {
			return false;
		}

		size_t file_size = static_cast<size_t>(li_size.QuadPart);
		if (file_size < FILE_MIN_SIZE_SCAN || file_size > FILE_MAX_SIZE_SCAN) {
			return false;
		}

		HANDLE mapping_handle = CreateFileMappingW(file_handle, nullptr,
			PAGE_READONLY, 0, 0, nullptr);
		if (!mapping_handle) {
			return false;
		}
		defer{ CloseHandle(mapping_handle); };

		unsigned char* data = static_cast<unsigned char*>(
			MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0));
		if (!data) {
			return false;
		}
		defer{ UnmapViewOfFile(data); };

		size_t sample_size = std::min<size_t>(1024, file_size);
		std::span<unsigned char> sample(data, sample_size);

		// Helper lambda: verify both sample and full file with one checkFn
		auto verify = [&](auto&& checkFn) -> bool {
			if (!checkFn(sample)) return false;
			// Sample passed, now read full file
			std::span<unsigned char> buffer(data, file_size);
			return checkFn(buffer);
			};

		// Try encodings in order
		if (verify(CheckPrintableUTF8))  return true;
		if (verify(CheckPrintableUTF16)) return true;
		//if (verify(CheckPrintableUTF32)) return true;

		return false;
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

	std::vector<std::string> TrID::GetTypes(const std::wstring& file_path)
	{
		std::lock_guard<std::mutex> lock(m_mutex); // TRID is not thread safe
		//PrintDebugW(L"Getting types of file: %ws", file_path.wstring().c_str());
		
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

		const auto file_path_str = ulti::WstrToStr(file_path);

		char buf[256]; // Buffer for TrID API results
		try
		{
			trid_api->SubmitFileA(file_path_str.c_str());
			ret = trid_api->Analyze();
			if (ret)
			{
				ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use
				//PrintDebugW(L"TrID analysis successful for file %ws, result code: %d", file_path.c_str(), ret);
				ret = trid_api->GetInfo(TRID_GET_RES_NUM, 0, buf);
				for (int i = ret + 1; i--;)
				{
					ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use

					try { trid_api->GetInfo(TRID_GET_RES_FILETYPE, i, buf); }
					catch (...) { continue; }
					
					std::string type_str(buf);
					if (type_str.size() == 0
						|| type_str.find("ransom") != std::string::npos
						|| type_str.find("ncrypt") != std::string::npos) 
						continue;

					ZeroMemory(buf, sizeof(buf)); // Clear buffer for next use
					
					try { trid_api->GetInfo(TRID_GET_RES_FILEEXT, i, buf); }
					catch (...) { continue; }

					std::string ext_str(buf);

					if (ext_str.size() > 0)
					{
						ulti::ToLowerOverride(ext_str);
						std::stringstream ss(ext_str);
						std::string ext;
						while (std::getline(ss, ext, '/'))
						{
							types.push_back(ext); // Save found extension
						}
					}
					else if (type_str.size() > 0)
					{
						ulti::ToLowerOverride(type_str);
						types.push_back(type_str);
					}
				}
			}
		}
		catch (...)
		{

		}

		// Read all bytes of the file
		if (IsPrintableFile(file_path))
		{
			types.push_back("txt");
		}

		if (types.size() == 0)
		{
			types.push_back("");
		}

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
