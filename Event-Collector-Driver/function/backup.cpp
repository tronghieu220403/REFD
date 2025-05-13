#include "backup.h"
#include "../std/file/file.h"   

namespace backup
{
    bool BackupFile(const WCHAR* file_path,
        WCHAR* backup_path,
        ull backup_path_max_len,
        ull* backup_path_len,
        const PFLT_FILTER p_filter_handle, const PFLT_INSTANCE p_instance, const PFILE_OBJECT p_file_object = nullptr)
    {
        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            return false;
        }
        ull backup_hash = 0;
        for (ull i = 0; file_path[i] != L'\0'; ++i)
        {
            backup_hash += (backup_hash * 65535 + file_path[i]) % (10000000007);
        }
        if (backup_path == nullptr)
        {
            return false;
        }

        // Merge the backup path and backup name
        if (!NT_SUCCESS(::RtlStringCchPrintfW(backup_path, backup_path_max_len, L"%s%lld", BACKUP_PATH, backup_hash)))
        {
            ZeroMemory(backup_path, backup_path_max_len);
            DebugMessage("RtlStringCchPrintfW failed\n");
            return false;
        }

        file::FileFlt src_file(file_path, p_filter_handle, p_instance, p_file_object);
        if (!src_file.Open())
        {
            DebugMessage("Open file %ws failed\n", file_path);
            return false;
        }

        ull src_file_size = src_file.Size();
        if (src_file_size == 0)
        {
            DebugMessage("File %ws size is 0\n", file_path);
            return false;
        }

        UCHAR* buffer = new UCHAR[BEGIN_WIDTH + END_WIDTH];
        if (buffer == nullptr)
        {
            backup_path_len = 0;
            backup_path[0] = L'\0';
            return false;
        }

        file::FileFlt dst_file(backup_path, p_filter_handle, p_instance);
        
        // Backup file already exists
        if (dst_file.Exist())
        {
            DebugMessage("Backup file %ws already exists\n", backup_path);
            return true;
        }

        if (dst_file.Open() == false)
        {
            DebugMessage("Open backup file %ws failed\n", backup_path);
            backup_path_len = 0;
            backup_path[0] = L'\0';
            return false;
        }

        if (src_file_size < BEGIN_WIDTH + END_WIDTH)
        {
            if (src_file.ReadWithOffset(buffer, src_file_size, 0) != src_file_size)
            {
                delete[] buffer;
                backup_path_len = 0;
                backup_path[0] = L'\0';
                DebugMessage("Read file %ws failed\n", file_path);
                return false;
            }
        }
        else
        {
            if (src_file.ReadWithOffset(buffer, BEGIN_WIDTH, 0) != BEGIN_WIDTH 
                || src_file.ReadWithOffset(&buffer[BEGIN_WIDTH], END_WIDTH, src_file_size - END_WIDTH) != END_WIDTH
                )
            {
                delete[] buffer;
                backup_path_len = 0;
                backup_path[0] = L'\0';
                DebugMessage("Read file %ws failed\n", file_path);
                return false;
            }
        }

        ull size_to_write = min(src_file_size, BEGIN_WIDTH + END_WIDTH);
        DebugMessage("Write file %ws, size %lld\n", backup_path, size_to_write);
        if (dst_file.Append(buffer, size_to_write) == false)
        {
            backup_path_len = 0;
            backup_path[0] = L'\0';
            DebugMessage("Write file %ws failed\n", backup_path);
            return false;
        }

        delete[] buffer;

        if (backup_path_len != nullptr)
        {
            *backup_path_len = wcsnlen(backup_path, backup_path_max_len);
        }

        return true;
    }
}

