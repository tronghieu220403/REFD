#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef MANAGER_FILE_TYPE_IDEN_H_
#define MANAGER_FILE_TYPE_IDEN_H_

#include "../ulti/include.h"
#ifdef _M_IX86
#include "../trid/trid.h"
#endif // _M_IX86

#define HIEUNT_ERROR_FILE_NOT_FOUND_STR "HIEUNT_ERROR_FILE_NOT_FOUND_STR"

namespace type_iden
{
    //inline std::unordered_map<std::string, std::vector<std::string>> kExtensionMap = {
    //    { "7z", { "7z" } },
    //    { "apk", { "zip", "jar", "apk" } },
    //    { "bin", { "o", "elf executable and linkable format (linux)" } },
    //    { "bmp", { "bmp", "rle", "dib" } },
    //    { "css", { "txt", "" } },
    //    { "csv", { "txt" } },
    //    { "dll", { "exe", "icl", "" } },
    //    { "doc", { "abr", "generic ole2 / multistream compound", "doc" } },
    //    { "docx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "docx", "tmdx", "tmvx" } },
    //    { "dwg", { "dwg" } },
    //    { "so", { "o", "elf executable and linkable format (linux)" } },
    //    { "eps", { "eps", "ept", "ps", "txt" } },
    //    { "epub", { "zip", "zip document container (generic)", "epub" } },
    //    { "exe", { "exe", "icl", "vxd", "dll", "cpl" } },
    //    { "gif", { "gif" } },
    //    { "gz", { "gz", "gzip" } },
    //    { "html", { "html", "txt", "htm" } },
    //    { "ics", { "ics", "vcs", "txt" } },
    //    { "js", { "txt", "s" } },
    //    { "jpg", { "mp3", "jpg", "jpeg", "jpg/jpeg" } },
    //    { "json", { "txt" } },
    //    { "webp", { "philips respironics m-series data format", "generic riff container", "webp" } },
    //    { "pdf", { "pdf", "txt", "" } },
    //    { "mkv", { "mkv", "mka" } },
    //    { "mp3", { "mp3" } },
    //    { "mp4", { "abr", "mp4" } },
    //    { "ods", { "zip", "zip document container (generic)", "xmind", "ods" } },
    //    { "oxps", { "zip", "oxps", "xps" } },
    //    { "png", { "png" } },
    //    { "ppt", { "abr", "generic ole2 / multistream compound" } },
    //    { "pptx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "pptx" } },
    //    { "ps1", { "txt" } },
    //    { "rar", { "rar" } },
    //    { "svg", { "xml", "svg", "txt" } },
    //    { "tar", { "tar", "ustar" } },
    //    { "tif", { "tif", "tiff" } },
    //    { "txt", { "txt" } },
    //    { "xls", { "abr", "generic ole2 / multistream compound", "xls" } },
    //    { "xlsx", { "bin", "macbin", "generic ole2 / multistream compound", "zip", "xlsx" } },
    //    { "xml", { "xml", "txt", "html" } },
    //    { "zip", { "zip" } },
    //    { "zlib", { "zlib compressed data (fast comp.)" } }
    //};

    class FileType
    {
    private:
#ifdef _M_IX86
        TrID* trid_ = nullptr;
#endif // _M_IX86

    public:
        FileType() = default;
        ~FileType();

        bool InitTrid(const std::wstring& defs_dir, const std::wstring& trid_dll_path);

        std::vector<std::string> GetTypes(const std::wstring& file_path, DWORD* p_status, ull* file_size);
    };

    bool HasCommonType(const std::vector<std::string>& types1, const std::vector<std::string>& types2);

    std::wstring CovertTypesToString(const std::vector<std::string>& types);

}

inline type_iden::FileType* kFileType = nullptr;

#endif // MANAGER_FILE_TYPE_IDEN_H_
