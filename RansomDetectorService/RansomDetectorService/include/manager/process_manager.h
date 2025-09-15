#ifndef PROCESS_MANAGER_H_
#define PROCESS_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "file_manager.h"

#define INTERVAL_SECONDS_PROCESS_EVALUATION 15

namespace manager {

    struct ProcessInfo {
        size_t pid = 0;
        bool is_first_evaluation = true;
    };

	bool KillProcessByPID(DWORD pid);
}
#endif  // PROCESS_MANAGER_H_
