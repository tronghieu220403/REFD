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
        if (backup_path_len != nullptr)
        {
            *backup_path_len = 0;
        }
        if (backup_path_max_len > 0 && backup_path != nullptr)
        {
            backup_path[0] = L'\0';
        }

        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            return false;
        }
        if (backup_path == nullptr)
        {
            return false;
        }

        NTSTATUS status;

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
        status = src_file.Open();
        if (!NT_SUCCESS(status))
        {
            DebugMessage("Open file %ws failed", file_path);
            return false;
        }

        ull src_file_size = src_file.Size();
        if (src_file_size == 0 || src_file_size == ULL_MAX)
        {
			DebugMessage("File %ws size is %llu", file_path, src_file_size);
			return false;
        }

        Vector<UCHAR> vector_buffer;
        vector_buffer.Resize(BEGIN_WIDTH + END_WIDTH);
        UCHAR* buffer = vector_buffer.Data();
        if (buffer == nullptr)
        {
            return false;
        }

        if (src_file_size < BEGIN_WIDTH + END_WIDTH)
        {
            if (src_file.ReadWithOffset(buffer, src_file_size, 0) != src_file_size)
            {
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
                DebugMessage("Read file %ws failed", file_path);
                return false;
            }
        }
        ull size_to_write = min(src_file_size, BEGIN_WIDTH + END_WIDTH);

        file::FileFlt dst_file(backup_path, p_filter_handle, p_instance, nullptr, FILE_CREATE);
        status = dst_file.Open();
        if (status == STATUS_OBJECT_NAME_COLLISION)
        {
            //DebugMessage("File %ws already exist", backup_path);
            goto backup_file_return_true;
        }
        else if (status == STATUS_INVALID_DEVICE_OBJECT_PARAMETER)
        {
            // Backup path is on a different device/volume, so we need to create a new file.
            // file::FileFlt dst_file(backup_path, p_filter_handle, nullptr, nullptr, FILE_CREATE); will trigger a BSOD
            // Using ZwCreateFile instead of FltCreateFile to create a new file.
            file::ZwFile zw_dst_file;
            status = zw_dst_file.Open(backup_path, FILE_CREATE);
            if (status == STATUS_OBJECT_NAME_COLLISION)
            {
                //DebugMessage("File %ws already exist", backup_path);
                goto backup_file_return_true;
            }
            else if (!NT_SUCCESS(status))
            {
                //DebugMessage("Open backup file %ws failed", backup_path);
                return false;
            }
            if (zw_dst_file.Append(buffer, size_to_write) == false)
            {
                DebugMessage("Write file %ws failed", backup_path);
                return false;
            }
            else
            {
                //DebugMessage("Write file %ws success", backup_path);
            }
        }
        else if (!NT_SUCCESS((status)))
        {
            DebugMessage("Open backup file %ws failed", backup_path);
            return false;
        }
        else
        {
            //DebugMessage("Open backup file %ws success", backup_path);
            //DebugMessage("Write file %ws, size %lld", backup_path, size_to_write);
            if (dst_file.Append(buffer, size_to_write) == false)
            {
                DebugMessage("Write file %ws failed", backup_path);
                return false;
            }
        }
    backup_file_return_true:
        if (backup_path_len != nullptr)
        {
            *backup_path_len = wcsnlen(backup_path, backup_path_max_len);
        }

        return true;
    }
}

