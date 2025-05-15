#ifndef PROCESS_MANAGER_H_
#define PROCESS_MANAGER_H_

#include "../ulti/support.h"
#include "../ulti/debug.h"
#include "file_manager.h"

namespace manager {

    struct ProcessInfo {
        size_t pid = 0;
        size_t deleted_count = 0;
        size_t created_write_count = 0;
        size_t overwrite_count = 0;
    };
}
#endif  // PROCESS_MANAGER_H_
