#ifndef BACKUP_H
#define BACKUP_H
#include <fltKernel.h>
#include "../../std/string/string.h"
#include "../../std/sync/mutex.h"
#include "../../template/register.h"
#include "../../template/flt-ex.h"
#include "detector.h"

#define BACKUP_PATH L"\\??\\E:\\hieunt210330\\backup\\"

namespace backup
{
    bool BackupFile(const WCHAR* file_path, 
        WCHAR* backup_path,
        ull backup_path_max_len,
        ull* backup_path_len,
        const PFLT_FILTER p_filter_handle, const PFLT_INSTANCE p_instance, const PFILE_OBJECT p_file_object);
} // namespace backup

#endif // BACKUP_H