#ifndef MANAGER_FILE_MANAGER_H_
#define MANAGER_FILE_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

/* // TRID will not accept the extended path
#define MAIN_DIR L"\\\\?\\E:\\hieunt210330"
#define TEMP_DIR L"\\\\?\\E:\\hieunt210330\\backup\\"
*/
#define MAIN_DIR L"E:\\hieunt210330"
#define TEMP_DIR L"E:\\hieunt210330\\backup\\"

#define EVALUATATION_INTERVAL_MS 5000
#define EVALUATATION_INTERVAL_SEC (EVALUATATION_INTERVAL_MS / 1000)

#define MIN_TOTAL_SIZE_CHECK_PER_SEC (0)// 0MB
#define MIN_TOTAL_SIZE_CHECK (MIN_TOTAL_SIZE_CHECK_PER_SEC * EVALUATATION_INTERVAL_SEC)

#define MIN_FILE_COUNT 5

#define MIN_DIR_COUNT 2

#define FILE_MAX_SIZE_SCAN (5 * 1024 * 1024) // 5MB
#define FILE_MIN_SIZE_SCAN 10

#define BEGIN_WIDTH 1024
#define END_WIDTH 1024
#define HIEUNT_MAX_PATH (1024)

#define TYPE_MATCH_NOT_EVALUATED 0
#define TYPE_MISMATCH 1
#define TYPE_HAS_COMMON 2
#define TYPE_NULL 3
#define TYPE_NOT_NULL 4
#define TYPE_NO_EVALUATION 5
#define TYPE_DONE_EVALUATION 6

#define DEVICE_CACHE_USAGE_COUNT_MAX 10
#define DEVICE_CACHE_USAGE_SECOND_MAX 10

#define FILE_IO_FILTER_QUEUE_SIZE_MAX 10000
#define FILE_CACHE_SIZE_MAX 1000

namespace manager {

	struct RawFileIoInfo
	{
		ULONG requestor_pid = 0;
		bool is_modified = false;
		bool is_renamed = false;
		bool is_created = false;
		bool is_deleted = false;
		WCHAR path[HIEUNT_MAX_PATH] = { 0 };
	};

	struct FileIoInfo
	{
		ULONG requestor_pid = 0;
		bool is_modified = false;
		bool is_renamed = false;
		bool is_created = false;
		bool is_deleted = false;
		std::wstring path;
	};

	class FileIoManager {
	private:
		std::queue<FileIoInfo> file_io_queue_;
	public:
		
		std::mutex file_io_mutex_;

		void LockMutex();
		void UnlockMutex();

		FileIoInfo PopFileIoEvent();
		ull GetQueueSize();

		void MoveQueue(std::queue<FileIoInfo>& target_file_io_queue);

		void PushFileEventToQueue(const RawFileIoInfo* raw_file_io_info);

	};

	struct FileCacheInfo
	{
		ull size;
		vector<string> types;
	};

	class FileCache {
	private:
		// Internal node to track FileCacheInfo and its timestamp iterator
		struct Node {
			FileCacheInfo info;
			std::multimap<ull, ull>::iterator time_it; // iterator in time_index_
		};

		// Primary data containers
		std::unordered_map<ull, Node> cache_;  // hash -> Node
		std::multimap<ull, ull> time_index_;   // time -> hash (sorted ascending)

		// Separate locks for higher concurrency
		mutable std::shared_mutex mt_cache_;   // protects cache_
		mutable std::shared_mutex mt_time_;    // protects time_index_
		void RemoveOldestUnlocked();
	public:
		bool Add(const wstring& path, const FileCacheInfo& info);
		bool Get(const wstring& path, FileCacheInfo& info);
		bool Erase(const wstring& path);
	};

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
#endif  // MANAGER_FILE_MANAGER_H_
