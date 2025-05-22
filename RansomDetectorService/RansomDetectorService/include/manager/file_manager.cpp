﻿#include "file_manager.h"

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
		if (whitelist_pid_set_.find(raw_file_io_info->requestor_pid) != whitelist_pid_set_.end())
		{
			return;
		}
        FileIoInfo file_io_info;
        
        PrintDebugW(L"File I/O event before: requestor_pid: %d, is_modified: %d, is_deleted: %d, is_created: %d, is_renamed: %d, current_path: %ws, new_path: %ws, backup_name: %ws", raw_file_io_info->requestor_pid,(int)raw_file_io_info->is_modified, (int)raw_file_io_info->is_deleted, (int)raw_file_io_info->is_created, (int)raw_file_io_info->is_renamed, raw_file_io_info->current_path, raw_file_io_info->new_path, raw_file_io_info->backup_name);

        file_io_info.requestor_pid = raw_file_io_info->requestor_pid;
        file_io_info.is_modified = raw_file_io_info->is_modified;
        file_io_info.is_deleted = raw_file_io_info->is_deleted;
        file_io_info.is_created = raw_file_io_info->is_created;
        file_io_info.is_renamed = raw_file_io_info->is_renamed;
        file_io_info.path_list.push_back(std::move(ulti::ToLower(manager::GetDosPath(raw_file_io_info->current_path))));
        std::wstring backup_name = raw_file_io_info->backup_name;
        if (backup_name.empty() && raw_file_io_info->is_modified == true)
        {
            backup_name = std::to_wstring(GetPathHash(raw_file_io_info->current_path));
        }
        file_io_info.backup_name_list.push_back(backup_name);

        if (raw_file_io_info->is_renamed == true)
        {
            file_io_info.path_list.push_back(std::move(ulti::ToLower(manager::GetDosPath(raw_file_io_info->new_path))));
            file_io_info.backup_name_list.push_back(L"");
        }
        file_io_queue_.push(std::move(file_io_info));
    }

	bool FileIoManager::IsPidInWhiteList(ULONG pid)
	{
		return whitelist_pid_set_.find(pid) != whitelist_pid_set_.end();
	}

	void FileIoManager::AddPidToWhitelist(ULONG pid)
	{
		whitelist_pid_set_.insert(pid);
	}

	void FileIoManager::RemovePidToWhitelist(ULONG pid)
	{
		whitelist_pid_set_.erase(pid);
	}

    void InitDosDeviceCache()
    {
        wchar_t device_path[MAX_PATH];

        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            std::wstring drive_str = std::wstring(1, drive) + L":";
            if (QueryDosDeviceW(drive_str.c_str(), device_path, MAX_PATH)) {
                std::wstring device_name = device_path;
                kDosPath.insert({device_name, drive_str});
                kNativePath.insert({ drive_str, device_name });
                PrintDebugW(L"Cached: %ws -> %ws", device_name.c_str(), drive_str.c_str());
            }
        }
    }

    std::wstring GetNativePath(const std::wstring& dos_path)
    {
        std::wstring drive_name = dos_path.substr(0, dos_path.find_first_of('\\'));
        if (kNativePath.find(drive_name) != kNativePath.end())
        {
            return kNativePath[drive_name] + dos_path.substr(drive_name.length());
        }

        std::wstring device_name;
        device_name.resize(MAX_PATH);
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
        kNativePath.insert({ drive_name, device_name });
        return device_name + dos_path.substr(dos_path.find_first_of('\\'));
    }

    std::wstring GetDosPathCaseSensitive(const std::wstring& nt_path)
    {
        // If the file_path is empty or it does not start with "\\" (not a device file_path), return as-is
        if (nt_path.empty() || nt_path[0] != L'\\') {
            return nt_path;
        }

        // Remove Win32 Device Namespace or File Namespace prefix if present
        std::wstring clean_path = nt_path;
        if (clean_path.find(L"\\\\?\\") == 0 || clean_path.find(L"\\\\.\\") == 0) {
            return clean_path.substr(4); // Remove "\\?\" or "\\.\"
        }

        std::wstring device_name = nt_path.substr(0, nt_path.find(L'\\', sizeof("\\Device\\") - 1)); // Extract device name
        auto it = kDosPath.find(device_name);
        if (it != kDosPath.end()) {
            return it->second + clean_path.substr(device_name.length());
        }

        // Try all drive letters to find the matching device path
        wchar_t device_path[MAX_PATH];
        std::wstring dos_path = L"";
        for (wchar_t drive = L'A'; drive <= L'Z'; ++drive) {
            std::wstring drive_str = std::wstring(1, drive) + L":";
            if (QueryDosDeviceW(drive_str.c_str(), device_path, MAX_PATH)) {
                std::wstring device_name = device_path;
                if (clean_path.find(device_name) == 0) {
                    dos_path = drive_str + clean_path.substr(device_name.length());
                    kDosPath.insert({device_name, drive_str}); // Cache
                    break;
                }
            }
        }

        return dos_path;
    }

    std::wstring GetDosPath(const std::wstring& nt_path)
    {
        return ulti::ToLower(GetDosPathCaseSensitive(nt_path));
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
        return std::move(ulti::ToLower(std::move(file_name.substr(dot_pos + 1)))); // Return the file extension
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

    std::wstring CopyToTmp(const std::wstring& file_path, bool create_new_if_duplicate)
	{
		std::wstring base_tmp_name = std::to_wstring(GetPathHash(file_path));
        std::wstring dest = TEMP_DIR + base_tmp_name;
        if (create_new_if_duplicate == false)
        {
            PrintDebugW(L"Copying file %ws to %ws", file_path.c_str(), dest.c_str());
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
            PrintDebugW(L"Copying file %ws to %ws", file_path.c_str(), dest.c_str());
        }
        HANDLE h_src = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_src == INVALID_HANDLE_VALUE)
        {
            PrintDebugW(L"CreateFile failed for file %ws, error %s", file_path.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
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
            PrintDebugW(L"GetFileSizeEx failed for file %ws, error %s, tmp_file_size %lld", file_path.c_str(), debug::GetErrorMessage(GetLastError()).c_str(), tmp_file_size);
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
                PrintDebugW(L"ReadFile or WriteFile failed for file %ws, error %s", file_path.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
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
                PrintDebugW(L"ReadFile or WriteFile failed for file %ws, error %s", file_path.c_str(), debug::GetErrorMessage(GetLastError()).c_str());
				CloseHandle(h_src);
				CloseHandle(h_dest);
				return L"";
			}
		}

		// Close both files after completion
		CloseHandle(h_src);
		CloseHandle(h_dest);
        PrintDebugW(L"File %ws copied to %ws", file_path.c_str(), dest.c_str());
		return base_tmp_name;
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