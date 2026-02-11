#include "file_helper.h"

namespace helper
{
    void InitDosDeviceCache()
    {
        wchar_t device_path[MAX_PATH];

        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            std::wstring drive_str = std::wstring(1, drive) + L":";
            if (QueryDosDeviceW(drive_str.c_str(), device_path, MAX_PATH)) {
                std::wstring device_name = device_path;
                kDosPathCache.insert({device_name, drive_str});
                kNativePathCache.insert({ drive_str, device_name });
                PrintDebugW(L"Cached: %ws -> %ws", device_name.c_str(), drive_str.c_str());
            }
        }
    }

    std::wstring GetNativePath(const std::wstring& dos_path)
    {
        std::wstring drive_name = dos_path.substr(0, dos_path.find_first_of('\\'));
        
        using namespace std::chrono;

        auto now = steady_clock::now();
        defer{ kLastNativeQueryTime = now; };
        auto elapsed = duration_cast<seconds>(now - kLastNativeQueryTime).count();
        bool allow_cache = (elapsed < DEVICE_CACHE_USAGE_SECOND_MAX && kNativeOpCnt < DEVICE_CACHE_USAGE_COUNT_MAX);
        if (allow_cache && kNativePathCache.find(drive_name) != kNativePathCache.end())
        {
            ++kNativeOpCnt;
            return kNativePathCache[drive_name] + dos_path.substr(drive_name.length());
        }

        std::wstring device_name;
        device_name.resize(30);
        DWORD status;
        while (QueryDosDeviceW(drive_name.data(), (WCHAR*)device_name.data(), (DWORD)device_name.size()) == 0)
        {
            status = GetLastError();
            if (status != ERROR_INSUFFICIENT_BUFFER)
            {
                PrintDebugW(L"QueryDosDevice failed for Win32 file_path %ws, error %s", dos_path.c_str(), debug::GetErrorMessage(status).c_str());
                return std::wstring();
            }
            device_name.resize(device_name.size() * 2);
        }
        device_name.resize(wcslen(device_name.data()));
        kNativePathCache.insert({ drive_name, device_name });

        kNativeOpCnt = 0;

        return device_name + dos_path.substr(dos_path.find_first_of('\\'));
    }

    std::wstring GetDosPathCaseSensitive(const std::wstring& nt_path)
    {
        using namespace std::chrono;

        // If the ws is empty or it does not start with "\\" (not a device ws), return as-is
        if (nt_path.empty() || nt_path[0] != L'\\') {
            return nt_path;
        }

        // Remove Win32 Device Namespace or File Namespace prefix if present
        std::wstring clean_path = nt_path;
        if (clean_path.find(L"\\\\?\\") == 0 || clean_path.find(L"\\\\.\\") == 0) {
            return clean_path.substr(4); // Remove "\\?\" or "\\.\"
        }

        auto now = steady_clock::now();
        defer{ kLastDosQueryTime = now; };
        auto elapsed = duration_cast<seconds>(now - kLastDosQueryTime).count();
        bool allow_cache = (elapsed < DEVICE_CACHE_USAGE_SECOND_MAX && kDosOpCnt < DEVICE_CACHE_USAGE_COUNT_MAX);

        if (allow_cache)
        {
            std::wstring device_name = nt_path.substr(0, nt_path.find(L'\\', sizeof("\\Device\\") - 1)); // Extract device name
            auto it = kDosPathCache.find(device_name);
            if (it != kDosPathCache.end())
            {
                ++kDosOpCnt;
                return it->second + clean_path.substr(device_name.length());
            }
        }

        // Try all drive letters to find the matching device path
        wchar_t device_path[MAX_PATH];
        std::wstring dos_path = L"";
        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            std::wstring drive_str = std::wstring(1, drive) + L":";
            if (QueryDosDeviceW(drive_str.c_str(), device_path, MAX_PATH)) {
                std::wstring device_name(ulti::ToLower(device_path));
                if (ulti::ToLower(clean_path).find(device_name) == 0) {
                    dos_path = drive_str + clean_path.substr(device_name.length());
                    kDosPathCache.insert({device_name, drive_str}); // Cache
                    break;
                }
            }
        }
        kDosOpCnt = 0;
        return dos_path;
    }

    std::wstring GetDosPath(const std::wstring& nt_path)
    {
        return ulti::ToLower(GetDosPathCaseSensitive(nt_path));
    }

    std::wstring GetLongDosPath(const std::wstring& dos_path)
    {
        if (dos_path.empty()) {
            return dos_path;
        }
        else if (dos_path.find(L'~') == std::wstring::npos) {
            return dos_path;
        }
        std::wstring long_path;
        long_path.resize(32767);
        DWORD result = GetLongPathNameW(dos_path.c_str(), &long_path[0], (DWORD)long_path.size());
        long_path.resize(result);
        if (result == 0 || result > 3767) {
            return dos_path;
        }
        return long_path;
    }

    bool FileExist(const std::wstring& ws)
    {
        DWORD file_attributes = GetFileAttributesW(ws.c_str());
        if (file_attributes == INVALID_FILE_ATTRIBUTES || FlagOn(file_attributes, FILE_ATTRIBUTE_DIRECTORY) == true) {
            return false;
        }
        return true;
    }

    bool DirExist(const std::wstring& dir_path)
    {
        DWORD file_attributes = GetFileAttributesW(dir_path.c_str());
        if (file_attributes == INVALID_FILE_ATTRIBUTES || FlagOn(file_attributes, FILE_ATTRIBUTE_DIRECTORY) == false) {
            return false;
        }
        return true;
    }

    ull GetFileSize(const std::wstring& ws)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesEx(ws.c_str(), GetFileExInfoStandard, &fad))
        {
            PrintDebugW(L"GetFileSize failed for file %ws, error %s", ws.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
            return 0;
        }
        LARGE_INTEGER size;
        size.HighPart = fad.nFileSizeHigh;
        size.LowPart = fad.nFileSizeLow;
        return size.QuadPart;
    }

    std::wstring GetFileName(const std::wstring& path)
    {
        size_t pos = path.find_last_of(L"\\/");
        if (pos == std::string::npos) return path;
        return path.substr(pos + 1);
    }

    std::wstring GetFileNameNoExt(const std::wstring& path)
    {
        auto file_name = GetFileName(path);
        size_t dot = file_name.find_first_of(L'.');
        if (dot == std::wstring::npos)
            return file_name;
        return file_name.substr(0, dot);
    }

    // Function to get file extension
    std::wstring GetFileExtension(const std::wstring& file_name) {
		size_t name_pos = file_name.find_last_of(L"\\/");
        size_t dot_pos = file_name.find_last_of(L".");
        if (dot_pos == std::wstring::npos || name_pos > dot_pos) {
            return L""; // No file extension
        }
        return std::move(ulti::ToLower(std::move(file_name.substr(dot_pos + 1)))); // Return the file extension
    }

    std::vector<std::wstring> GetFileExtensions(const std::wstring& file_name)
    {
        std::vector<std::wstring> extensions;

        size_t name_pos = file_name.find_last_of(L"\\/");
        std::wstring just_name = (name_pos != std::wstring::npos) ? file_name.substr(name_pos + 1) : file_name;

        // Đếm số dấu chấm
        size_t first_dot = just_name.find(L'.');
        if (first_dot == std::wstring::npos) {
            return extensions;
        }

        size_t start = first_dot + 1;
        while (start < just_name.length()) {
            size_t next_dot = just_name.find(L'.', start);
            std::wstring ext;
            if (next_dot == std::wstring::npos) {
                ext = just_name.substr(start);
            }
            else {
                ext = just_name.substr(start, next_dot - start);
            }

            extensions.push_back(ulti::ToLower(std::move(ext)));

            if (next_dot == std::wstring::npos) break;
            start = next_dot + 1;
        }

        return extensions;
    }

    ull GetWstrHash(const std::wstring& ws)
    {
        const ull FNV_OFFSET = 1469598103934665603ULL;
        const ull FNV_PRIME = 1099511628211ULL;

        ull hash = FNV_OFFSET;
        for (wchar_t c : ws)
        {
            hash ^= static_cast<ull>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    std::wstring CopyToTmp(const std::wstring& ws, bool create_new_if_duplicate)
	{
		std::wstring base_tmp_name = std::to_wstring(GetWstrHash(ws));
        std::wstring dest = TEMP_DIR + base_tmp_name;
        if (create_new_if_duplicate == false)
        {
            PrintDebugW(L"Copying file %ws to %ws", ws.c_str(), dest.c_str());
            if (FileExist(dest) == true)
            {
                PrintDebugW(L"File %ws already exists", dest.c_str());
                return base_tmp_name;
            }
        }
        else
        {
            // Create a unique name for the destination file
            std::wstring base_name = dest;
            std::wstring tmp_name = base_tmp_name;
            int counter = 1;
            while (FileExist(dest) == true)
            {
                tmp_name = base_tmp_name + L"_" + std::to_wstring(counter);
                dest = base_name + L"_" + std::to_wstring(counter);
                counter++;
            }
            base_tmp_name = tmp_name;
            PrintDebugW(L"Copying file %ws to %ws", ws.c_str(), dest.c_str());
        }
        HANDLE h_src = CreateFileW(ws.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_src == INVALID_HANDLE_VALUE)
        {
            PrintDebugW(L"CreateFile failed for file %ws, error %s", ws.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
            return L"";
        }
        HANDLE h_dest = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_dest == INVALID_HANDLE_VALUE)
        {
            PrintDebugW(L"CreateFile failed for file %ws, error %s", dest.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
            CloseHandle(h_src);
            return L"";
        }
		ull tmp_file_size = 0;
		if (::GetFileSizeEx(h_src, (PLARGE_INTEGER)&tmp_file_size) == 0 || tmp_file_size > INT_MAX)
		{
            PrintDebugW(L"GetFileSizeEx failed for file %ws, error %s, tmp_file_size %lld", ws.c_str(), debug::GetErrorMessage(GetLastError()).c_str(), tmp_file_size);
			return L"";
		}
		std::vector<UCHAR> data;
		data.resize((std::min)((size_t)tmp_file_size, (size_t)BEGIN_WIDTH + END_WIDTH));
		// Read and write the first BEGIN_WIDTH bytes and last END_WIDTH bytes
		DWORD bytes_cnt = 0;

		if (tmp_file_size < BEGIN_WIDTH + END_WIDTH) {
			// If the file is smaller than BEGIN_WIDTH + END_WIDTH, read the entire file
			if (!ReadFile(h_src, data.data(), (DWORD)tmp_file_size, &bytes_cnt, NULL) || bytes_cnt != tmp_file_size 
				|| !WriteFile(h_dest, data.data(), bytes_cnt, &bytes_cnt, NULL) || bytes_cnt != tmp_file_size)
			{
                PrintDebugW(L"ReadFile or WriteFile failed for file %ws, error %s", ws.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
				CloseHandle(h_src);
				CloseHandle(h_dest);
				return L"";
			}
		}
		else {
			// Read the first BEGIN_WIDTH bytes
			SetFilePointer(h_src, 0, NULL, FILE_BEGIN);
			if (!ReadFile(h_src, data.data(), BEGIN_WIDTH, &bytes_cnt, NULL) || bytes_cnt != BEGIN_WIDTH
				|| !WriteFile(h_dest, data.data(), bytes_cnt, &bytes_cnt, NULL) || bytes_cnt != BEGIN_WIDTH
				|| SetFilePointer(h_src, (LONG)tmp_file_size - END_WIDTH, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER
				|| !ReadFile(h_src, data.data(), END_WIDTH, &bytes_cnt, NULL) || bytes_cnt != END_WIDTH
				|| !WriteFile(h_dest, data.data(), bytes_cnt, &bytes_cnt, NULL) || bytes_cnt != END_WIDTH
				)
			{
                PrintDebugW(L"ReadFile or WriteFile failed for file %ws, error %s", ws.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
				CloseHandle(h_src);
				CloseHandle(h_dest);
				return L"";
			}
		}

		// Close both files after completion
		CloseHandle(h_src);
		CloseHandle(h_dest);
        PrintDebugW(L"File %ws copied to %ws", ws.c_str(), dest.c_str());
		return base_tmp_name;
    }

    // Function to clear temporary files
    void ClearTmpFiles() {
        std::filesystem::path tmp_dir = TEMP_DIR;
        // Convert ws to wstring for Windows API
        std::wstring temp_path = tmp_dir.wstring();
        if (temp_path[temp_path.size() - 1] != L'\\') {
            temp_path += L"\\";
        }

        // Set up filter to get all files and directories in the temporary directory
        std::wstring search_path = temp_path + L"*";
        WIN32_FIND_DATAW find_file_data;
        HANDLE h_find = FindFirstFileW(search_path.c_str(), &find_file_data);

        if (h_find == INVALID_HANDLE_VALUE) {
            return;  // Cannot open temporary directory
        }

        do {
            // Skip special files and directories "." and ".."
            if (wcscmp(find_file_data.cFileName, L".") == 0 || wcscmp(find_file_data.cFileName, L"..") == 0) {
                continue;
            }

            // Create full ws to file or directory
            std::wstring full_path = temp_path + find_file_data.cFileName;

            // Check and delete file or directory
            if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // If it'ws a directory, delete it (not recursive)
                RemoveDirectoryW(full_path.c_str());
            }
            else {
                // If it'ws a file, delete it
                DeleteFileW(full_path.c_str());
            }
        } while (FindNextFileW(h_find, &find_file_data) != 0);

        // Close search handle
        FindClose(h_find);
    }
}