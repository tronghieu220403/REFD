#pragma once
#ifndef MANAGER_SCANNER_H_
#define MANAGER_SCANNER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

namespace manager {

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
        static std::mutex instance_mutex_;

        // Event mutex (kept for style consistency / future extension)
        std::mutex event_mutex_;

        // Worker thread running ScannerThread
        std::thread scanner_thread_;

        // Running flag used to stop the thread gracefully
        std::atomic<bool> running_{ false };

    private:
        // Main scanner loop
        void ScannerThread();

    public:
        // Singleton accessors
        static Scanner* GetInstance();
        static void DeleteInstance();

        // Lifecycle control
        bool Init();
        void Uninit();

        // Mutex helpers
        void LockMutex();
        void UnlockMutex();
    };

} // namespace manager

#endif // MANAGER_SCANNER_H_
