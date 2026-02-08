#include "scanner.h"
#include "receiver.h"
#include "file_type_iden.h"
#include "ulti/file_helper.h"
#include "receiver.h"

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
        // Local queue used to minimize lock holding time
        std::queue<FileIoInfo> local_queue;
        std::set<ull> file_hash_scanned;

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
                local_queue.push(std::move(tmp_queue.front()));
                tmp_queue.pop();
            }
            size_t n = local_queue.size();
            // Process events
            for (size_t i = 0; i < n; i++)
            {
                if (local_queue.size() == 0) {
                    break;
                }
                FileIoInfo info = std::move(local_queue.front());
                local_queue.pop();
                
                auto lp = ulti::ToLower(info.path);

                if (IsPathWhitelisted(lp) == true) {
                    continue;
                }

                auto hash = helper::GetWstrHash(lp);
                if (file_hash_scanned.find(hash) != file_hash_scanned.end()) { // Scanned -> skip
                    continue;
                }

                DWORD status = ERROR_SUCCESS;
                ull file_size = 0;

                auto types = ft->GetTypes(info.path, &status, &file_size);
                if (status == ERROR_SHARING_VIOLATION) {
                    if (rcv != nullptr) {
                        rcv->LockMutex();
                        rcv->PushFileEvent(std::move(info.path), info.pid);
                        rcv->UnlockMutex();
                    }
                    continue;
                }
                auto types_wstr = ulti::StrToWstr(ulti::JoinStrings(types, ","));
                auto time_ms = ulti::GetCurrentSteadyTimeInMs();
                PrintDebugW(L"%ws,(%ws)", info.path.c_str(), types_wstr.c_str());
                debug::WriteLogW(L"%lld,%ws,(%ws)\n", time_ms, info.path.c_str(), types_wstr.c_str());

                file_hash_scanned.insert(hash);
            }

            // Sleep briefly to avoid busy spinning
            Sleep(50);
        }
    }

} // namespace manager
