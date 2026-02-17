#pragma once
#ifndef MANAGER_SCANNER_H_
#define MANAGER_SCANNER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "receiver.h"
#include "../ulti/lru_cache.hpp"

namespace manager {

    inline const std::vector<std::wstring> kPathWhitelist = {
#ifdef _DEBUG
        L"c:\\windows\\",
        L"c:\\program files\\",
        L"c:\\program files (x86)\\",
        L"c:\\programdata\\",
        L"\\appdata\\local\\",
#endif // _DEBUG
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

        struct ScheduledPath {
            ull time_scan_ms = 0;
            std::wstring path;
        };

        struct ScheduledPathCompare {
            bool operator()(const ScheduledPath& lhs, const ScheduledPath& rhs) const
            {
                return lhs.time_scan_ms > rhs.time_scan_ms;
            }
        };
        std::unordered_map<ULONG, std::priority_queue<ScheduledPath, std::vector<ScheduledPath>, ScheduledPathCompare>> pid_queues_;

#ifdef DEBUG
        static constexpr size_t kWorkerCount = 4;
#else
        static constexpr size_t kWorkerCount = 2;
#endif // DEBUG

        std::vector<std::thread> worker_threads_;
        std::condition_variable cv_;
        std::mutex file_scan_state_mutex_;

        struct FileScanState {
            ull last_scan_ms = 0;
            bool has_pending_rescan = false;
        };
        LruMap<ull, FileScanState> file_scan_states_{ 100'000 };

    private:
        void ResendToPidQueue(FileIoInfo&& io);
        bool TryQueueByCooldown(ULONG pid, std::wstring&& path, ull now_ms);
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
