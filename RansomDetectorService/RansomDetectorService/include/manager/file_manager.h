#ifndef FILE_MANAGER_H_
#define FILE_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

#define MAIN_DIR L"E:\\"
#define TEMP_DIR L"E:\\backup\\"

#define EVALUATATION_INTERVAL_MS 5000
#define EVALUATATION_INTERVAL_SEC (EVALUATATION_INTERVAL_MS / 1000)

#define MIN_TOTAL_SIZE_CHECK_PER_SEC (0)// 0MB
#define MIN_TOTAL_SIZE_CHECK (MIN_TOTAL_SIZE_CHECK_PER_SEC * EVALUATATION_INTERVAL_SEC)

#define MIN_FILE_COUNT_PER_SEC 2
#define MIN_FILE_COUNT (MIN_FILE_COUNT_PER_SEC * EVALUATATION_INTERVAL_SEC)
#define MAX_FILE_COUNT (MIN_FILE_COUNT * 5)

#define MIN_DIR_COUNT 2

#define FILE_MAX_SIZE_SCAN (30 * 1024 * 1024) // 30MB

#define THRESHOLD_PERCENTAGE 80
#define BelowThreshold(part, total) (part <= total * THRESHOLD_PERCENTAGE / 100)

#define BEGIN_WIDTH 1024
#define END_WIDTH 1024
#define HIEUNT_MAX_PATH (260 * 4)

namespace manager {

	struct RawFileIoInfo
	{
		ULONG requestor_pid = 0;
		bool is_modified = false;
		bool is_deleted = false;
		bool is_created = false;
		bool is_renamed = false;
		WCHAR current_path[HIEUNT_MAX_PATH] = { 0 };
		WCHAR new_path[HIEUNT_MAX_PATH] = { 0 };
		WCHAR backup_name[HIEUNT_MAX_PATH] = { 0 };
	};

	struct FileIoInfo
	{
		ULONG requestor_pid = 0;
		bool is_modified = false;
		bool is_deleted = false;
		bool is_created = false;
		bool is_renamed = false;
		bool is_success = true;
		std::wstring current_path;
		std::wstring current_backup_name;
		std::vector<std::wstring> new_path_list;
		std::wstring new_backup_name;
		std::vector<std::string> current_types;
		std::vector<std::string> new_types;
	};

	class FileIoManager {
	private:
		std::mutex file_io_mutex_;
		std::queue<FileIoInfo> file_io_queue_;
	public:
		void LockMutex();
		void UnlockMutex();

		FileIoInfo PopFileIoEvent();
		uint64_t GetQueueSize();

		void MoveQueue(std::queue<FileIoInfo>& target_file_io_queue);

		void PushFileEventToQueue(const RawFileIoInfo* raw_file_io_info);
	};

	/*___________________________________________*/

	inline std::unordered_map<std::wstring, const std::wstring> kNativePath;
	inline std::unordered_map<std::wstring, const std::wstring> kWin32Path;

	/*_________________FUNCTIONS_________________*/

	// DOS path getter function
	std::wstring GetNativePath(const std::wstring& win32_path);

	// Win32 path getter function
	std::wstring GetWin32PathCaseSensitive(const std::wstring& path);
	std::wstring GetWin32Path(const std::wstring& path);

	bool FileExist(const std::wstring& file_path);
	bool DirExist(const std::wstring& dir_path);

	uint64_t GetFileSize(const std::wstring& file_path);

	std::wstring GetFileExtension(const std::wstring& file_name);

	ull GetPathHash(const std::wstring& file_path);
	
	std::wstring CopyToTmp(const std::wstring& file_path);

	void ClearTmpFiles();

	std::vector<std::pair<std::wstring, std::vector<std::string>>> GetTypes(const std::vector<std::wstring>& file_list);

}
#endif  // FILE_MANAGER_H_
