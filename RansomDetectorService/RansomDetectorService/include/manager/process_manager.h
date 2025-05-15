#ifndef PROCESS_MANAGER_H_
#define PROCESS_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "file_manager.h"

namespace manager {

    struct ProcessInfo {
        uint64_t pid = 0;
    };

    class ProcessManager {
    public:
        
		void LockMutex();
        void UnlockMutex();

    private:
        std::unordered_map<uint64_t, ProcessInfo> process_map_; // PID -> ProcessInfo
        std::mutex process_map_mutex_;
    };
}
#endif  // PROCESS_MANAGER_H_
