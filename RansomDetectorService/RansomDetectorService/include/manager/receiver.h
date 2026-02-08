#ifndef MANAGER_RECEIVER_H_
#define MANAGER_RECEIVER_H_

#include "ulti/support.h"
#include "ulti/debug.h"
#include "ulti/lru_cache.hpp"

#define MAX_NAME_CACHE_SIZE 1'000'000

namespace manager {

	struct FileIoInfo {
		ULONG pid;
		std::wstring path;
	};

	class Receiver {
	private:
		std::queue<FileIoInfo> file_io_queue_;
		std::mutex file_io_mutex_;

		LruMap<ULONGLONG, std::wstring> name_cache_{ MAX_NAME_CACHE_SIZE };

	public:
		// Singleton
		static Receiver* GetInstance();

		// Lifecycle
		bool Init();
		void Uninit();

		// Queue ops
		void LockMutex();
		void UnlockMutex();

		FileIoInfo PopFileIoEvent();
		ull GetQueueSize();

		void MoveQueue(std::queue<FileIoInfo>& target_file_io_queue);

		void PushFileEvent(const std::wstring& path, ULONG pid);
	};

}
#endif  // MANAGER_RECEIVER_H_
