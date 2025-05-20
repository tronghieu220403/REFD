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
        if (backup_path == nullptr)
        {
            return false;
        }

		ull backup_hash = 0;
		for (ull i = 0; file_path[i] != L'\0'; ++i)
		{
			backup_hash += (backup_hash * 65535 + file_path[i]) % (10000000007);
		}

        // Merge the backup path and backup name
        if (!NT_SUCCESS(::RtlStringCchPrintfW(backup_path, backup_path_max_len, L"%s%lld", BACKUP_PATH, backup_hash)))
        {
            ZeroMemory(backup_path, backup_path_max_len);
            DebugMessage("RtlStringCchPrintfW failed");
            return false;
        }

        file::FileFlt src_file(file_path, p_filter_handle, p_instance, p_file_object);
        if (!src_file.Open())
        {
            DebugMessage("Open file %ws failed", file_path);
            return false;
        }

        ull src_file_size = src_file.Size();
        if (src_file_size == 0 || src_file_size == ULL_MAX)
        {
			DebugMessage("Flt*: file %ws size is %llu", file_path, src_file_size);
			src_file_size = file::IoGetFileSize(file_path);
			if (src_file_size == 0 || src_file_size == ULL_MAX)
			{
				DebugMessage("Io*: file %ws size is %llu", file_path, src_file_size);
				return false;
			}
        }

		DebugMessage("File %ws size is %llu", file_path, src_file_size);

        UCHAR* buffer = new UCHAR[BEGIN_WIDTH + END_WIDTH];
        if (buffer == nullptr)
        {
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
                DebugMessage("Read file %ws failed", file_path);
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
                DebugMessage("Read file %ws failed", file_path);
                return false;
            }
        }

        file::FileFlt dst_file(backup_path, p_filter_handle, p_instance, nullptr, FILE_OPEN_IF);

        // Backup file already exists
        if (dst_file.Exist())
        {
            DebugMessage("Backup file %ws already exists", backup_path);
            return true;
        }

        if (dst_file.Open() == false)
        {
            DebugMessage("Open backup file %ws failed", backup_path);
            backup_path_len = 0;
            backup_path[0] = L'\0';
            return false;
        }
        else
        {
            DebugMessage("Open backup file %ws success", backup_path);
        }

        ull size_to_write = min(src_file_size, BEGIN_WIDTH + END_WIDTH);
        DebugMessage("Write file %ws, size %lld", backup_path, size_to_write);
        if (dst_file.Append(buffer, size_to_write) == false)
        {
            backup_path_len = 0;
            backup_path[0] = L'\0';
            DebugMessage("Write file %ws failed", backup_path);
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

