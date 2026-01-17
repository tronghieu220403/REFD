#ifndef MANAGER_RECEIVER_H_
#define MANAGER_RECEIVER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "com/minifilter_comm.h"

#define PORT_NAME L"\\hieunt_mf"
#define MAX_QUEUE_SIZE 100000

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

		std::thread collector_thread_;
		std::atomic<bool> running_{ false };

	public:
		// Singleton
		static Receiver* GetInstance();
		static void DeleteInstance();

		static void ReceiverThread();

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
		void PushFileEventFromBuffer(const void* buffer, size_t buffer_size);
	};

}
#endif  // MANAGER_RECEIVER_H_
