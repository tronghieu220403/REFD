#include "process_manager.h"

namespace manager
{
    void ProcessManager::LockMutex()
    {
        process_map_mutex_.lock();
    }

    void ProcessManager::UnlockMutex()
    {
        process_map_mutex_.unlock();
    }
}
