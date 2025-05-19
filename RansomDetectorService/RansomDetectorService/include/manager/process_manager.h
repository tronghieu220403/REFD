#ifndef PROCESS_MANAGER_H_
#define PROCESS_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "file_manager.h"

#define INTERVAL_MINUTE_PROCESS_EVALUATION 1

namespace manager {

    struct ProcessInfo {
        size_t pid = 0;
        size_t deleted_count = 0;
        size_t created_write_count = 0;
        size_t overwrite_count = 0;
        size_t overwrite_mismatch_count = 0;
        size_t true_deleted_count = 0;
        size_t created_write_null_count = 0;
        size_t last_index = 0;
        std::chrono::steady_clock::time_point last_evaluation_time =
            std::chrono::steady_clock::now(); // force trigger immediately
        bool is_first_evaluation = true;
    };
}
#endif  // PROCESS_MANAGER_H_
