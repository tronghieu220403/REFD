#include "scanner.h"
#include "receiver.h"
#include "file_type_iden.h"
#include "ulti/file_helper.h"

namespace manager {
    namespace {
        constexpr ull kRescanDelayMs = 2ULL * 60ULL * 1000ULL;
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

    void Scanner::ResendToPidQueue(FileIoInfo&& io, ull next_scan_ms)
    {
        {
            // Protect per-PID queue structure
            std::lock_guard<std::mutex> lk(pid_queue_mutex_);

            // Push the file back to its original PID queue
            pid_queues_[io.pid].push({ next_scan_ms, std::move(io.path) });
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

            auto lp = ulti::ToLower(io.path);
            auto hash = helper::GetWstrHash(lp);
            auto now_ms = ulti::GetCurrentSteadyTimeInMs();

            {
                std::lock_guard<std::mutex> g(file_scan_state_mutex_);
                FileScanState state;
                auto exist = file_scan_states_.get(hash, state);
                //PrintDebugW("[TID %d] exist %d, state.last_scan_ms %lld, state.next_scan_ms %lld, path %ws", tid, exist, state.last_scan_ms, state.next_scan_ms, lp.c_str());
                if (exist == true && now_ms <= state.last_scan_ms + kRescanDelayMs) {
                    if (state.next_scan_ms <= now_ms) {
                        state.next_scan_ms = now_ms + kRescanDelayMs;
                        ResendToPidQueue(std::move(io), state.next_scan_ms);
                        file_scan_states_.put(hash, state);
                    }
                    continue;
                }
                state.last_scan_ms = now_ms;
                file_scan_states_.put(hash, state);
            }

            //PrintDebugW("[TID %d] Scaning %ws, pid %d", tid, io.path.c_str(), io.pid);

            DWORD status = ERROR_SUCCESS;
            ull file_size = 0;

            if (helper::DirExist(io.path) == true) {
                PrintDebugW(L"[Scan TID %d] PID %d, %ws, d", tid, io.pid, io.path.c_str());
                debug::WriteLogW(L"%lld,d,%ws\n", now_ms, io.path.c_str());
                continue;
            }

            auto types = ft->GetTypes(io.path, &status, &file_size);
            if (status == ERROR_SHARING_VIOLATION) {
                //PrintDebugW("[TID %d] Resend %ws, pid %d", tid, io.path.c_str(), io.pid);
                ResendToPidQueue(std::move(io), now_ms + kRescanDelayMs * 2);
                continue;
            }

            std::wstring types_wstr = ulti::StrToWstr(ulti::JoinStrings(types, ","));
            PrintDebugW(L"[Scan TID %d] PID %d, %ws, (%ws), %d", tid, io.pid, io.path.c_str(), types_wstr.c_str(), status);
            if (types_wstr.empty() == false) {
                debug::WriteLogW(L"%lld,f,%ws,(%ws),%d\n", now_ms, io.path.c_str(), types_wstr.c_str(), status);
                continue;
            }
            if (status != ERROR_SUCCESS) {
                debug::WriteLogW(L"%lld,e,%ws,%d\n", now_ms, io.path.c_str(), types_wstr.c_str(), status);
                continue;
            }
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

                const ull now_ms = ulti::GetCurrentSteadyTimeInMs();
                while (!tmp_queue.empty()) {
                    FileIoInfo ele = std::move(tmp_queue.front());
                    if (IsPathWhitelisted(ele.path) == false) {
                        //PrintDebugW("Push path to pid_queues_: %ws", ele.path.c_str());
                        pid_queues_[ele.pid].push({ now_ms, std::move(ele.path) });
                    }
                    tmp_queue.pop();
                }

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
                            //PrintDebugW("Push path to file_queues_: %ws", io.path.c_str());
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
