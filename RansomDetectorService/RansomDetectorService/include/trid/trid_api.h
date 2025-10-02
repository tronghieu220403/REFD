#pragma once

#ifndef TRID_TRID_API_H
#define TRID_TRID_API_H

#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifdef _M_IX86

#include <windows.h>

#define TRID_GET_RES_NUM          1     // Get the number of results available
#define TRID_GET_RES_FILETYPE     2     // Filetype descriptions
#define TRID_GET_RES_FILEEXT      3     // Filetype extension
#define TRID_GET_RES_POINTS       4     // Matching points

#define TRID_GET_VER              1001  // TrIDLib version (major * 100 + minor)
#define TRID_GET_DEFSNUM          1004  // Number of filetypes definitions loaded


// Additional constants for the full version

#define TRID_GET_DEF_ID           100   // Get the id of the filetype's definition for a given result
#define TRID_GET_DEF_FILESCANNED  101   // Various info about that def
#define TRID_GET_DEF_AUTHORNAME   102   //     "
#define TRID_GET_DEF_AUTHOREMAIL  103   //     "
#define TRID_GET_DEF_AUTHORHOME   104   //     "
#define TRID_GET_DEF_FILE         105   //     "
#define TRID_GET_DEF_REMARK       106   //     "
#define TRID_GET_DEF_RELURL       107   //     "
#define TRID_GET_DEF_TAG          108   //     "
#define TRID_GET_DEF_MIMETYPE     109   //     "

#define TRID_GET_ISTEXT           1005  // Check if the submitted file is text or binary one

#define TRID_MISSING_LIBRARY      12321

class TridApi
{

	int(__stdcall* pAnalyze)();
	int(__stdcall* pGetInfo)(int info_type, int info_idx, char* out);
	int(__stdcall* pLoadDefsPack)(const char* file_name);
	int(__stdcall* pSubmitFileA)(const char* file_name);

	HMODULE hm;

public:
	int __stdcall Analyze();
	int __stdcall GetInfo(int info_type, int info_idx, char* out);
	int __stdcall LoadDefsPack(const char* file_name);
	int __stdcall SubmitFileA(const char* file_name);
	TridApi(const char* defs_path = nullptr, const wchar_t* trid_lib_dll_path = nullptr);
	~TridApi();
};

#endif // _M_IX86

#endif // TRID_TRID_API_H