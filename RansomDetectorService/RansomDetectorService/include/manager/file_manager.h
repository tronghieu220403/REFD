#ifndef FILE_MANAGER_H_
#define FILE_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

#define MAIN_DIR L"\\??\\E:\\hieunt210330"
#define TEMP_DIR L"\\??\\E:\\hieunt210330\\backup\\"

#define EVALUATATION_INTERVAL_MS 5000
#define EVALUATATION_INTERVAL_SEC (EVALUATATION_INTERVAL_MS / 1000)

#define MIN_TOTAL_SIZE_CHECK_PER_SEC (0)// 0MB
#define MIN_TOTAL_SIZE_CHECK (MIN_TOTAL_SIZE_CHECK_PER_SEC * EVALUATATION_INTERVAL_SEC)

#define MIN_FILE_COUNT 5

#define MIN_DIR_COUNT 2

#define FILE_MAX_SIZE_SCAN (30 * 1024 * 1024) // 30MB

#define THRESHOLD_PERCENTAGE 80
#define BelowThreshold(part, total) (part <= total * THRESHOLD_PERCENTAGE / 100)

#define BEGIN_WIDTH 1024
#define END_WIDTH 1024
#define HIEUNT_MAX_PATH (260 * 4)

#define TYPE_MATCH_NOT_EVALUATED 0
#define TYPE_MISMATCH 1
#define TYPE_HAS_COMMON 2
#define TYPE_NULL 3
#define TYPE_NOT_NULL 4
#define TYPE_NO_EVALUATION 5

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
		ULONG type_match = TYPE_MATCH_NOT_EVALUATED;
		std::vector<std::wstring> path_list;
		std::vector<std::wstring> backup_name_list;
		std::vector<std::string> old_types;
		std::vector<std::string> new_types;
	};

	class FileIoManager {
	private:
		std::mutex file_io_mutex_;
		std::queue<FileIoInfo> file_io_queue_;
		std::unordered_set<ULONG> whitelist_pid_set_;
	public:
		void LockMutex();
		void UnlockMutex();

		FileIoInfo PopFileIoEvent();
		uint64_t GetQueueSize();

		void MoveQueue(std::queue<FileIoInfo>& target_file_io_queue);

		void PushFileEventToQueue(const RawFileIoInfo* raw_file_io_info);

		bool IsPidInWhiteList(ULONG pid);
		void AddPidToWhitelist(ULONG pid);
		void RemovePidToWhitelist(ULONG pid);

	};

	/*___________________________________________*/

	inline std::unordered_map<std::wstring, const std::wstring> kNativePath;
	inline std::unordered_map<std::wstring, const std::wstring> kDosPath;

	/*_________________FUNCTIONS_________________*/

	void InitDosDeviceCache();

	// DOS path getter function
	std::wstring GetNativePath(const std::wstring& dos_path);

	// Win32 path getter function
	std::wstring GetDosPathCaseSensitive(const std::wstring& nt_path);
	std::wstring GetDosPath(const std::wstring& nt_path);

	bool FileExist(const std::wstring& file_path);
	bool DirExist(const std::wstring& dir_path);

	uint64_t GetFileSize(const std::wstring& file_path);

	std::wstring GetFileExtension(const std::wstring& file_name);

	ull GetPathHash(const std::wstring& file_path);
	
	std::wstring CopyToTmp(const std::wstring& file_path, bool create_new_if_duplicate = false);

	void ClearTmpFiles();

	std::vector<std::pair<std::wstring, std::vector<std::string>>> GetTypes(const std::vector<std::wstring>& file_list);

}
#endif  // FILE_MANAGER_H_
