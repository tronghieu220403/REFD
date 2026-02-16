#include "scanner.h"
#include "receiver.h"
#include "file_type_iden.h"
#include "ulti/file_helper.h"

namespace manager {
    namespace {
        constexpr ull kRescanDelayMs = 5ULL * 60ULL * 1000ULL;
    }

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

        scanner_thread_ = std::thread(&Scanner::QueuingThread, this);

        for (size_t i = 0; i < kWorkerCount; i++) {
            worker_threads_.emplace_back(&Scanner::WorkerThread, this);
        }

        return true;
    }

    void Scanner::Uninit()
    {
        // Signal the thread to stop
        running_ = false;
        cv_.notify_all();

        // Wait for the thread to exit cleanly
        if (scanner_thread_.joinable())
            scanner_thread_.join();

        for (auto& t : worker_threads_) {
            if (t.joinable())
                t.join();
        }
        worker_threads_.clear();
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

    void Scanner::ResendToPidQueue(FileIoInfo&& io)
    {
        {
            // Protect per-PID queue structure
            std::lock_guard<std::mutex> lk(pid_queue_mutex_);

            // Push the file back to its original PID queue
            pid_queues_[io.pid].push({ ulti::GetCurrentSteadyTimeInMs() + kRescanDelayMs, std::move(io.path) });
        }
    }

    void Scanner::WorkerThread()
    {
        auto ft = type_iden::FileType::GetInstance();
        if (!ft) return;

        auto tid = GetCurrentThreadId();

        while (running_)
        {
            FileIoInfo io;
            {
                std::unique_lock<std::mutex> lk(file_queue_mutex_);
                cv_.wait(lk, [&] {
                    return !running_ || !file_queues_.empty();
                    });

                if (!running_)
                    return;

                io = std::move(file_queues_.front());
                file_queues_.pop();
            }

            //PrintDebugW("[TID %d] Scaning %ws, pid %d", tid, io.path.c_str(), io.pid);

            auto lp = ulti::ToLower(io.path);
            auto hash = helper::GetWstrHash(lp);

            {
                std::lock_guard<std::mutex> g(file_hash_mutex_);
                if (file_hash_scanned_.count(hash))
                {
                    //PrintDebugW("[TID %d] Scanned %ws, pid %d", tid, io.path.c_str(), io.pid);
                    continue;
                }
                file_hash_scanned_.insert(hash);
            }

            DWORD status = ERROR_SUCCESS;
            ull file_size = 0;

            auto types = ft->GetTypes(io.path, &status, &file_size);
            if (status == ERROR_SHARING_VIOLATION) {
                //PrintDebugW("[TID %d] Resend %ws, pid %d", tid, io.path.c_str(), io.pid);
                ResendToPidQueue(std::move(io));
                continue;
            }

            std::wstring types_wstr = ulti::StrToWstr(ulti::JoinStrings(types, ","));
            auto time_ms = ulti::GetCurrentSteadyTimeInMs();
            PrintDebugW(L"[TID %d] %ws,(%ws)", tid, io.path.c_str(), types_wstr.c_str());
            debug::WriteLogW(L"%lld,%ws,(%ws)\n", time_ms, io.path.c_str(), types_wstr.c_str());
        }
    }

    void Scanner::QueuingThread()
    {
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

            rcv->MoveQueueSync(tmp_queue);
            int n_file_to_scan = 0;

            {
                std::lock_guard<std::mutex> lk(pid_queue_mutex_);

                while (!tmp_queue.empty()) {
                    FileIoInfo ele = std::move(tmp_queue.front());
                    pid_queues_[ele.pid].push({ ulti::GetCurrentSteadyTimeInMs(), std::move(ele.path) });
                    tmp_queue.pop();
                }

                const ull now_ms = ulti::GetCurrentSteadyTimeInMs();
                std::vector<ULONG> pids_to_remove;
                while (n_file_to_scan < kWorkerCount) {
                    if (pid_queues_.size() == 0) {
                        break;
                    }
                    bool any_file_available = false;
                    for (auto& it : pid_queues_) {
                        auto& q = it.second;
                        if (q.empty()) {
                            pids_to_remove.push_back(it.first);
                            continue;
                        }

                        if (q.top().time_scan_ms <= now_ms) {
                            FileIoInfo io{ it.first, q.top().path };
                            q.pop();

                            file_queue_mutex_.lock();
                            file_queues_.push(std::move(io));
                            file_queue_mutex_.unlock();
                            cv_.notify_one();

                            n_file_to_scan++;
                            any_file_available = true;
                        }
                    }

                    for (auto pid : pids_to_remove) {
                        pid_queues_.erase(pid);
                    }
                    
                    if (any_file_available == false) {
                        break;
                    }
                }
            }

            if (n_file_to_scan == 0) {
                // Sleep briefly to avoid busy spinning
                Sleep(100);
            }
        }
    }

} // namespace manager
