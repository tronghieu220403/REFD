#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_
#define ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_

#include "../ulti/include.h"
#include "../trid/trid_api.h"
#define PRODUCT_PATH L"E:\\hieunt210330\\"

#define BelowTextThreshold(part, total) (part <= total * 97 / 100)

#define HIEUNT_ERROR_FILE_NOT_FOUND_STR "HIEUNT_ERROR_FILE_NOT_FOUND"

namespace type_iden
{
	bool CheckPrintableUTF16(const std::vector<unsigned char>& buffer);

	bool CheckPrintableUTF8(const std::vector<unsigned char>& buffer);

	bool CheckPrintableANSI(const std::vector<unsigned char>& buffer);

	bool IsPrintableFile(const fs::path& file_path);

	// TrID lib can only run on x86 process. Do not use this in async multi-thread.
	// To use TrID on async multi-thread, each instance of TrID must create and load an unique clone of TrIDLib.dll (for example TrIDLib_X.dll)
	class TrID
	{
	private:
        std::mutex m_mutex;
        std::size_t issue_thread_id = 0;
        TridApi* trid_api = nullptr;
	public:
        TrID() = default;
        ~TrID();
        bool Init(const std::wstring& defs_dir, const std::wstring& trid_dll_path);
        std::vector<std::string> GetTypes(const fs::path& file_path);
	};

}

inline type_iden::TrID* kTrID = nullptr;

#endif
