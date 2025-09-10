#ifndef PROCESS_MANAGER_H_
#define PROCESS_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "file_manager.h"

#define INTERVAL_SECONDS_PROCESS_EVALUATION 15

namespace manager {

    struct ProcessInfo {
        size_t pid = 0;
        size_t folder_changed_count = 0;
        size_t file_changed_count = 0;
        size_t last_index = 0;
        std::chrono::steady_clock::time_point last_evaluation_time =
            std::chrono::steady_clock::now(); // force trigger immediately
        bool is_first_evaluation = true;
    };

	bool KillProcessByPID(DWORD pid);
}
#endif  // PROCESS_MANAGER_H_
