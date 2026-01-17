#pragma once
#ifndef MANAGER_SCANNER_H_
#define MANAGER_SCANNER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"

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
