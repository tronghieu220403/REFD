#include "scanner.h"
#include "receiver.h"
#include "file_type_iden.h"
#include "file_helper.h"

namespace manager {

    // ======================================================
    // Static members
    // ======================================================

    Scanner* Scanner::instance_ = nullptr;
    std::mutex Scanner::instance_mutex_;

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
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_)
            instance_ = new Scanner();
        return instance_;
    }

    void Scanner::DeleteInstance()
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_)
        {
            instance_->Uninit();
            delete instance_;
            instance_ = nullptr;
        }
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

            rcv->LockMutex();
            rcv->MoveQueue(local_queue);
            rcv->UnlockMutex();

            // Process events
            while (!local_queue.empty())
            {
                const FileIoInfo& info = local_queue.front();

                auto lp = ulti::ToLower(info.path);

                auto hash = helper::GetWstrHash(ulti::ToLower(info.path));
                if (file_hash_scanned.find(hash) != file_hash_scanned.end()) { // Scanned -> skip
                    continue;
                }

                DWORD status = ERROR_SUCCESS;
                ull file_size = 0;

                auto types = ft->GetTypes(info.path, &status, &file_size);
                auto types_wstr = ulti::StrToWstr(ulti::JoinStrings(types, ","));
                debug::WriteLogW(L"%ws,(%ws)\n", info.path.c_str(), types_wstr.c_str());

                file_hash_scanned.insert(hash);
                local_queue.pop();
            }

            // Sleep briefly to avoid busy spinning
            Sleep(50);
        }
    }

} // namespace manager
