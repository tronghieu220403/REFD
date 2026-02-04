#ifndef MANAGER_RECEIVER_H_
#define MANAGER_RECEIVER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

namespace manager {

	struct FileIoInfo {
		std::wstring path;
	};

	class Receiver {
	private:
		Receiver() = default;
		~Receiver() = default;

		static Receiver* instance_;
		static std::mutex instance_mutex_;

		std::queue<FileIoInfo> file_io_queue_;
		std::mutex file_io_mutex_;
	public:
		// Singleton
		static Receiver* GetInstance();
		static void DeleteInstance();

		// Lifecycle
		bool Init();
		void Uninit();

		// Queue ops
		void LockMutex();
		void UnlockMutex();

		FileIoInfo PopFileIoEvent();
		ull GetQueueSize();

		void MoveQueue(std::queue<FileIoInfo>& target_file_io_queue);

		void PushFileEvent(std::wstring& path);
	};

}
#endif  // MANAGER_RECEIVER_H_
