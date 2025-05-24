#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_
#define ETWSERVICE_MANAGER_FILE_TYPE_IDEN_H_

#include "../ulti/include.h"
#include "../trid/trid_api.h"
#define PRODUCT_PATH L"E:\\hieunt210330\\hieunt210330\\"

#define BelowTextThreshold(part, total) (part <= total * 97 / 100)

#define HIEUNT_ERROR_FILE_NOT_FOUND_STR "HIEUNT_ERROR_FILE_NOT_FOUND_STR"

namespace type_iden
{
    inline std::unordered_map<std::string, std::vector<std::string>> kExtensionMap = {
        { "7z", { "7z" } },
        { "apk", { "zip", "jar", "apk" } },
        { "bin", { "o", "elf executable and linkable format (linux)" } },
        { "bmp", { "bmp", "rle", "dib" } },
        { "css", { "txt", "" } },
        { "csv", { "txt" } },
        { "dll", { "exe", "icl", "" } },
        { "doc", { "abr", "generic ole2 / multistream compound", "doc" } },
        { "docx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "docx", "tmdx", "tmvx" } },
        { "dwg", { "dwg" } },
        { "so", { "o", "elf executable and linkable format (linux)" } },
        { "eps", { "eps", "ept", "ps", "txt" } },
        { "epub", { "zip", "zip document container (generic)", "epub" } },
        { "exe", { "exe", "icl", "vxd", "dll", "cpl" } },
        { "gif", { "gif" } },
        { "gz", { "gz", "gzip" } },
        { "html", { "html", "txt", "htm" } },
        { "ics", { "ics", "vcs", "txt" } },
        { "js", { "txt", "s" } },
        { "jpg", { "mp3", "jpg", "jpeg", "jpg/jpeg" } },
        { "json", { "txt" } },
        { "webp", { "philips respironics m-series data format", "generic riff container", "webp" } },
        { "pdf", { "pdf", "txt", "" } },
        { "mkv", { "mkv", "mka" } },
        { "mp3", { "mp3" } },
        { "mp4", { "abr", "mp4" } },
        { "ods", { "zip", "zip document container (generic)", "xmind", "ods" } },
        { "oxps", { "zip", "oxps", "xps" } },
        { "png", { "png" } },
        { "ppt", { "abr", "generic ole2 / multistream compound" } },
        { "pptx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "pptx" } },
        { "ps1", { "txt" } },
        { "rar", { "rar" } },
        { "svg", { "xml", "svg", "txt" } },
        { "tar", { "tar", "ustar" } },
        { "tif", { "tif", "tiff" } },
        { "txt", { "txt" } },
        { "xls", { "abr", "generic ole2 / multistream compound", "xls" } },
        { "xlsx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "xlsx" } },
        { "xml", { "xml", "txt", "html" } },
        { "zip", { "zip" } },
        { "zlib", { "zlib compressed data (fast comp.)" } }
    };

    bool HasCommonType(const std::vector<std::string>& types1, const std::vector<std::string>& types2);

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

    std::wstring CovertTypesToString(const std::vector<std::string>& types);

}

inline type_iden::TrID* kTrID = nullptr;

#endif
