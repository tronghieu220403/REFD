#include "file_manager.h"

namespace manager
{
	void FileIoManager::LockMutex()
    {
        file_io_mutex_.lock();
    }

    void FileIoManager::UnlockMutex()
    {
        file_io_mutex_.unlock();
    }

    FileIoInfo FileIoManager::PopFileIoEvent()
    {
        FileIoInfo file_io_info;
        if (!file_io_queue_.empty())
        {
            file_io_info = file_io_queue_.front();
            file_io_queue_.pop();
        }
        return file_io_info;
    }

    uint64_t FileIoManager::GetQueueSize()
    {
        return file_io_queue_.size();
    }

	void FileIoManager::MoveQueue(std::queue<FileIoInfo>& target_file_io_queue)
	{
		target_file_io_queue = std::move(file_io_queue_);
	}

    void FileIoManager::PushFileEventToQueue(const RawFileIoInfo* raw_file_io_info)
    {
		
    }

    std::wstring GetNativePath(const std::wstring& win32_path)
    {
        std::wstring drive_name = win32_path.substr(0, win32_path.find_first_of('\\'));
        if (kNativePath.find(drive_name) != kNativePath.end())
        {
            return kNativePath[drive_name] + win32_path.substr(drive_name.length());
        }

        std::wstring device_name;
        device_name.resize(MAX_PATH);
        DWORD status;
        while (QueryDosDeviceW(drive_name.data(), (WCHAR*)device_name.data(), (DWORD)device_name.size()) == 0)
        {
            status = GetLastError();
            if (status != ERROR_INSUFFICIENT_BUFFER)
            {
                PrintDebugW(L"QueryDosDevice failed for Win32 file_path %ws, error %s", win32_path.c_str(), debug::GetErrorMessage(status).c_str());
                return std::wstring();
            }
            device_name.resize(device_name.size() * 2);
        }
        device_name.resize(wcslen(device_name.data()));
        kNativePath.insert({ drive_name, device_name });
        return device_name + win32_path.substr(win32_path.find_first_of('\\'));
    }

    std::wstring GetWin32PathCaseSensitive(const std::wstring& path)
    {
        // If the file_path is empty or it does not start with "\\" (not a device file_path), return as-is
        if (path.empty() || path[0] != L'\\') {
            return path;
        }

        // Remove Win32 Device Namespace or File Namespace prefix if present
        std::wstring clean_path = path;
        if (clean_path.find(L"\\\\?\\") == 0 || clean_path.find(L"\\\\.\\") == 0) {
            return clean_path.substr(4); // Remove "\\?\" or "\\.\"
        }

        std::wstring device_name = path.substr(0, path.find(L'\\', sizeof("\\Device\\") - 1)); // Extract device name
        auto it = kWin32Path.find(device_name);
        if (it != kWin32Path.end()) {
            return it->second + clean_path.substr(device_name.length());
        }

        // Prepare UNICODE_STRING for the native file_path
        UNICODE_STRING unicode_path = { (USHORT)(clean_path.length() * sizeof(wchar_t)), (USHORT)(clean_path.length() * sizeof(wchar_t)), (PWSTR)clean_path.c_str() };

        OBJECT_ATTRIBUTES obj_attr;
        InitializeObjectAttributes(&obj_attr, &unicode_path, OBJ_CASE_INSENSITIVE, NULL, NULL);

        HANDLE file_handle;
        IO_STATUS_BLOCK io_status;

        // Attempt to open the file or device
        NTSTATUS status = NtCreateFile(&file_handle, FILE_READ_ATTRIBUTES, &obj_attr, &io_status, NULL, FILE_ATTRIBUTE_READONLY, FILE_SHARE_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);

        if (!NT_SUCCESS(status)) {
            return L"";
        }

        // Get full file_path using the handle
        WCHAR buffer[MAX_PATH];
        DWORD result = GetFinalPathNameByHandleW(file_handle, buffer, MAX_PATH, FILE_NAME_NORMALIZED);
        CloseHandle(file_handle);

        std::wstring win_api_path;
        if (result != 0 && result < MAX_PATH) {
            win_api_path = std::wstring(buffer);
            if (win_api_path.find(L"\\\\?\\") == 0 || win_api_path.find(L"\\\\.\\") == 0) {
                win_api_path = win_api_path.substr(4); // Remove "\\?\" or "\\.\"
            }
            // Extract and cache the device prefix
            const std::wstring device_prefix = clean_path.substr(0, clean_path.find(L'\\', sizeof("\\Device\\") - 1));
            const std::wstring drive_prefix = win_api_path.substr(0, win_api_path.find_first_of(L'\\')); // Get the drive letter part (e.g., "C:")
            kWin32Path.insert({ device_name, drive_prefix }); // Cache the prefix mapping
        }
        else {
            win_api_path = L""; // Fallback to original file_path if conversion fails
        }

        return win_api_path;
    }

    std::wstring GetWin32Path(const std::wstring& path)
    {
        return ulti::ToLower(GetWin32PathCaseSensitive(path));
    }

    bool FileExist(const std::wstring& file_path)
    {
        DWORD file_attributes = GetFileAttributesW(file_path.c_str());
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

    uint64_t GetFileSize(const std::wstring& file_path)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesEx(file_path.c_str(), GetFileExInfoStandard, &fad))
        {
            PrintDebugW(L"GetFileSize failed for file %ws, error %s", file_path.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
            return 0;
        }
        LARGE_INTEGER size;
        size.HighPart = fad.nFileSizeHigh;
        size.LowPart = fad.nFileSizeLow;
        return size.QuadPart;
    }

    // Function to get file extension
    std::wstring GetFileExtension(const std::wstring& file_name) {
		size_t name_pos = file_name.find_last_of(L"\\/");
        size_t dot_pos = file_name.find_last_of(L".");
        if (dot_pos == std::wstring::npos || name_pos > dot_pos) {
            return L""; // No file extension
        }
        return ulti::ToLower(file_name.substr(dot_pos + 1)); // Return the file extension
    }

	ull GetPathHash(const std::wstring& file_path)
	{
		ull backup_hash = 0;
		for (auto& c : file_path)
		{
			backup_hash += (backup_hash * 65535 + c) % (10000000007);
		}
		return backup_hash;
	}

    std::wstring CopyToTmp(const std::wstring& file_path)
	{
		std::wstring tmp_name = std::to_wstring(GetPathHash(file_path));
        std::wstring dest = TEMP_DIR + tmp_name;
		if (FileExist(dest) == true)
		{
			return tmp_name;
		}
        HANDLE h_src = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_src == INVALID_HANDLE_VALUE)
        {
            return L"";
        }
        HANDLE h_dest = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_dest == INVALID_HANDLE_VALUE)
        {
            CloseHandle(h_src);
            return L"";
        }
		ull tmp_file_size = 0;
		if (::GetFileSizeEx(h_src, (PLARGE_INTEGER)&tmp_file_size) == 0 || tmp_file_size > INT_MAX)
		{
			return L"";
		}
		std::vector<UCHAR> data;
		data.resize(min((size_t)tmp_file_size, BEGIN_WIDTH + END_WIDTH));
		// Read and write the first BEGIN_WIDTH bytes and last END_WIDTH bytes
		DWORD bytes_cnt = 0;

		if (tmp_file_size < BEGIN_WIDTH + END_WIDTH) {
			// If the file is smaller than BEGIN_WIDTH + END_WIDTH, read the entire file
			if (!ReadFile(h_src, data.data(), (DWORD)tmp_file_size, &bytes_cnt, NULL) || bytes_cnt != tmp_file_size 
				|| !WriteFile(h_dest, data.data(), bytes_cnt, &bytes_cnt, NULL) || bytes_cnt != tmp_file_size)
			{
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
				CloseHandle(h_src);
				CloseHandle(h_dest);
				return L"";
			}
		}

		// Close both files after completion
		CloseHandle(h_src);
		CloseHandle(h_dest);

		return tmp_name;
    }

    // Function to clear temporary files
    void ClearTmpFiles() {
        std::filesystem::path tmp_dir = TEMP_DIR;
        // Convert file_path to wstring for Windows API
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

            // Create full file_path to file or directory
            std::wstring full_path = temp_path + find_file_data.cFileName;

            // Check and delete file or directory
            if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // If it's a directory, delete it (not recursive)
                RemoveDirectoryW(full_path.c_str());
            }
            else {
                // If it's a file, delete it
                DeleteFileW(full_path.c_str());
            }
        } while (FindNextFileW(h_find, &find_file_data) != 0);

        // Close search handle
        FindClose(h_find);
    }

	std::vector<std::pair<std::wstring, std::vector<std::string>>> GetTypes(const std::vector<std::wstring>& file_list) // -> vector<file_path, list of ext>>
	{
		// Remember to cache
		return std::vector<std::pair<std::wstring, std::vector<std::string>>>();
	}

}