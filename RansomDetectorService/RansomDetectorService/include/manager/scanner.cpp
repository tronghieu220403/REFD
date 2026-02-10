#include "scanner.h"
#include "receiver.h"
#include "file_type_iden.h"
#include "ulti/file_helper.h"

namespace manager {

    // ======================================================
    // Constructor / Destructor
    // ======================================================

    Scanner::Scanner()
    {
        // Constructor intentionally empty
    }

    Scanner::~Scanner()
    {
        // Destructor intentionally empty
    }

    // ======================================================
    // Singleton implementation
    // ======================================================

    Scanner* Scanner::GetInstance()
    {
        static Scanner instance;
        return &instance;
    }

    // ======================================================
    // Lifecycle management
    // ======================================================

    bool Scanner::Init()
    {
        auto ft = type_iden::FileType::GetInstance();
        if (ft == nullptr) {
            return false;
        }

        // Avoid starting the thread twice
        if (running_)
            return true;

        running_ = true;

        // Start scanner worker thread
        scanner_thread_ = std::thread(&Scanner::ScannerThread, this);
        return true;
    }

    void Scanner::Uninit()
    {
        // Signal the thread to stop
        running_ = false;

        // Wait for the thread to exit cleanly
        if (scanner_thread_.joinable())
            scanner_thread_.join();
    }

    // ======================================================
    // Mutex helpers
    // ======================================================

    void Scanner::LockMutex()
    {
        event_mutex_.lock();
    }

    void Scanner::UnlockMutex()
    {
        event_mutex_.unlock();
    }

    // ======================================================
    // Scanner worker thread
    // ======================================================

    void Scanner::ScannerThread()
    {
        std::set<ull> file_hash_scanned;
        std::unordered_map<ULONG, std::queue<std::wstring>> pid_queues;

        auto rcv = Receiver::GetInstance();
        if (rcv == nullptr) {
            return;
        }

        auto ft = type_iden::FileType::GetInstance();
        if (ft == nullptr) {
            return;
        }

        while (running_)
        {
            // Move all pending events from Receiver
            std::queue<FileIoInfo> tmp_queue;

            rcv->LockMutex();
            rcv->MoveQueue(tmp_queue);
            rcv->UnlockMutex();

            while (!tmp_queue.empty()) {
                FileIoInfo ele = std::move(tmp_queue.front());
                pid_queues[ele.pid].push(std::move(ele.path));
                tmp_queue.pop();
            }

            std::vector<ULONG> pids_to_remove;
            while (pid_queues.size() != 0) {
                for (auto& it : pid_queues) {
                    if (it.second.size() != 0) {
                        FileIoInfo io;
                        io.pid = it.first;
                        io.path = std::move(it.second.front());
                        file_queues_.push(std::move(io));
                        it.second.pop();
                    }
                    else {
                        pids_to_remove.push_back(it.first);
                    }
                }
                for (auto pid : pids_to_remove) {
                    pid_queues.erase(pid);
                }
            }

            size_t n = file_queues_.size();
            // Process events
            for (size_t i = 0; i < n; i++)
            {
                FileIoInfo io(std::move(file_queues_.front()));
                file_queues_.pop();
                
                auto lp = ulti::ToLower(io.path);

                //if (IsPathWhitelisted(lp) == true) {
                //    continue;
                //}

                auto hash = helper::GetWstrHash(lp);
                if (file_hash_scanned.find(hash) != file_hash_scanned.end()) { // Scanned -> skip
                    continue;
                }

                DWORD status = ERROR_SUCCESS;
                ull file_size = 0;

                auto types = ft->GetTypes(io.path, &status, &file_size);
                if (status == ERROR_SHARING_VIOLATION) {
                    pid_queues[io.pid].push(std::move(io.path));
                    continue;
                }
                auto types_wstr = ulti::StrToWstr(ulti::JoinStrings(types, ","));
                auto time_ms = ulti::GetCurrentSteadyTimeInMs();
                PrintDebugW(L"%ws,(%ws)", io.path.c_str(), types_wstr.c_str());
                debug::WriteLogW(L"%lld,%ws,(%ws)\n", time_ms, io.path.c_str(), types_wstr.c_str());

                file_hash_scanned.insert(hash);
            }

            if (rcv->GetQueueSize() == 0) {
                // Sleep briefly to avoid busy spinning
                Sleep(200);
            }
        }
    }

} // namespace manager
