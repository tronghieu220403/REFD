#ifndef ULTI_HELPER_H_
#define ULTI_HELPER_H_

#include "support.h"
#include "debug.h"

/* // TRID will not accept the extended path
#define MAIN_DIR L"\\\\?\\E:\\hieunt210330"
#define TEMP_DIR L"\\\\?\\E:\\hieunt210330\\backup\\"
*/
#define MAIN_DIR L"E:\\hieunt210330"
#define TEMP_DIR L"E:\\hieunt210330\\backup\\"

#define BEGIN_WIDTH 1024
#define END_WIDTH 1024
#define HIEUNT_MAX_PATH (1024)

#define DEVICE_CACHE_USAGE_COUNT_MAX 10
#define DEVICE_CACHE_USAGE_SECOND_MAX 10

namespace helper {

	/*___________________________________________*/

	inline int kNativeOpCnt = 0;
	inline int kDosOpCnt = 0;

	inline std::chrono::steady_clock::time_point kLastNativeQueryTime = std::chrono::steady_clock::now();
	inline std::chrono::steady_clock::time_point kLastDosQueryTime = std::chrono::steady_clock::now();

	inline std::unordered_map<std::wstring, const std::wstring> kNativePathCache;
	inline std::unordered_map<std::wstring, const std::wstring> kDosPathCache;

	/*_________________FUNCTIONS_________________*/

	void InitDosDeviceCache();

	// DOS path getter function
	std::wstring GetNativePath(const std::wstring& dos_path);

	// Win32 path getter function
	std::wstring GetDosPathCaseSensitive(const std::wstring& nt_path);
	std::wstring GetDosPath(const std::wstring& nt_path);
    std::wstring GetLongDosPath(const std::wstring& dos_path);

	bool FileExist(const std::wstring& file_path);
	bool DirExist(const std::wstring& dir_path);

	ull GetFileSize(const std::wstring& file_path);
	std::wstring GetFileName(const std::wstring& path);
	std::wstring GetFileNameNoExt(const std::wstring& path);

	std::wstring GetFileExtension(const std::wstring& file_name);
	std::vector<std::wstring> GetFileExtensions(const std::wstring& file_name);

	ull GetWstrHash(const std::wstring& file_path);
	
	std::wstring CopyToTmp(const std::wstring& file_path, bool create_new_if_duplicate = false);

	void ClearTmpFiles();
}
#endif  // ULTI_HELPER_H_
