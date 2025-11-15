#ifdef _M_IX86

#include "trid_api.h"

int __stdcall TridApi::Analyze()
{
	return pAnalyze();
}
int __stdcall TridApi::GetInfo(int info_type, int info_idx, char* out)
{
	return pGetInfo(info_type, info_idx, out);
}
int __stdcall TridApi::LoadDefsPack(const char* file_name)
{
	return pLoadDefsPack(file_name);
}
int __stdcall TridApi::SubmitFileA(const char* file_name)
{
	return pSubmitFileA(file_name);
}
TridApi::TridApi(const char* defs_path, const wchar_t* trid_lib_dll_path)
{
	hm = LoadLibraryW(trid_lib_dll_path ? trid_lib_dll_path : L"TrIDLib.dll");
	if (!hm)
		throw TRID_MISSING_LIBRARY;

	pAnalyze = (int(__stdcall*)())GetProcAddress(hm, "TrID_Analyze");
	pGetInfo = (int(__stdcall*)(int, int, char*))GetProcAddress(hm, "TrID_GetInfo");
	pLoadDefsPack = (int(__stdcall*)(const char*))GetProcAddress(hm, "TrID_LoadDefsPack");
	pSubmitFileA = (int(__stdcall*)(const char*))GetProcAddress(hm, "TrID_SubmitFileA");

	if (!(pAnalyze && pGetInfo && pLoadDefsPack && pSubmitFileA))
		throw TRID_MISSING_LIBRARY;

	if (defs_path)
		LoadDefsPack(defs_path);
}
TridApi::~TridApi()
{
	FreeLibrary(hm);
}

#endif // _M_IX86