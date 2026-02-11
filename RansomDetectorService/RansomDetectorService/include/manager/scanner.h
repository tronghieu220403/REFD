#pragma once
#ifndef MANAGER_SCANNER_H_
#define MANAGER_SCANNER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "receiver.h"

namespace manager {

    inline const std::vector<std::wstring> kPathWhitelist = {
        L"c:\\windows\\",
        L"c:\\program files\\",
        L"c:\\program files (x86)\\",
        L"c:\\programdata\\",
        L"\\appdata\\local\\",
    };

    // Check if path contains any whitelisted substring
    inline bool IsPathWhitelisted(const std::wstring& lower_path)
    {
        for (const auto& w : kPathWhitelist)
        {
            if (lower_path.find(w) != std::wstring::npos)
                return true;
        }
        return false;
    }

    class Scanner
    {
    private:
        // Private constructor/destructor to enforce Singleton
        Scanner();
        ~Scanner();

        // Disable copy & assignment
        Scanner(const Scanner&) = delete;
        Scanner& operator=(const Scanner&) = delete;

    private:
        // Singleton instance
        static Scanner* instance_;

        std::mutex event_mutex_;
        std::thread scanner_thread_;
        std::atomic<bool> running_{ false };

        std::mutex file_queue_mutex_;
        std::queue<FileIoInfo> file_queues_;
        std::mutex pid_queue_mutex_;
        std::unordered_map<ULONG, std::queue<std::wstring>> pid_queues_;

        static constexpr size_t kWorkerCount = 4;

        std::vector<std::thread> worker_threads_;
        std::condition_variable cv_;
        std::mutex file_hash_mutex_;
        std::set<ull> file_hash_scanned_;

    private:
        void ResendToPidQueue(FileIoInfo&& io);
        void QueuingThread();
        void WorkerThread();

    public:
        // Singleton accessors
        static Scanner* GetInstance();

        // Lifecycle control
        bool Init();
        void Uninit();

        // Mutex helpers
        void LockMutex();
        void UnlockMutex();
    };

} // namespace manager

#endif // MANAGER_SCANNER_H_
