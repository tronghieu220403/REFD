/*
Write a class in Windows Kernel Driver that have the following operation on a file: open, read, write, append, close. C++ std libraries is not allowed.
*/
#pragma once

#ifndef FILE_H
#define FILE_H

#include "../string/string.h"

#include <ntddk.h>

namespace file
{
    // SHOULD NOT USE INSIDE ANY MINIFILTER FUNCTION UNLESS NECCESSARY (for example access file on an another Harddisk).
    class ZwFile {
    private:
        String<WCHAR> file_path_;
        HANDLE file_handle_ = nullptr;
    public:
        ZwFile(const String<WCHAR>& current_path);
        ZwFile() = default;
        NTSTATUS Open(const WCHAR* file_path, ULONG create_disposition = FILE_OPEN_IF);

        ull Read(PVOID buffer, ull length);
        ull Append(PVOID buffer, ull length);
		ull Size();

        String<WCHAR> GetPath() const { return file_path_; }
        void Close();
        ~ZwFile();

    };

    // FltFile class using FltCreateFile, FltReadFile and FltWriteFile.
    class FileFlt
    {
    private:
        String<WCHAR> file_path_;
        PFILE_OBJECT p_file_object_ = nullptr;
        PFLT_FILTER p_filter_handle_ = { 0 };
        PFLT_INSTANCE p_instance_ = { 0 };
        HANDLE file_handle_ = nullptr;
		ULONG create_disposition_ = FILE_OPEN;
        bool is_open_ = false;
        bool pre_alloc_file_object_ = false; // if true, the caller must free the file object.
    public:
        FileFlt(const String<WCHAR>& current_path, const PFLT_FILTER p_filter_handle, const PFLT_INSTANCE p_instance, const PFILE_OBJECT p_file_object = nullptr, ULONG create_disposition = FILE_OPEN);
        NTSTATUS Open();
        void Close();

        bool IsOpen() const { return is_open_; }
        ull Read(PVOID buffer, ull length);
        ull ReadWithOffset(PVOID buffer, ull length, ull offset);
        bool Append(PVOID buffer, ull length);
        bool Exist();
        ull Size();
        ~FileFlt();

        String<WCHAR> GetPath() const { return file_path_; }

        void SetPfltFilter(PFLT_FILTER p_filter_handle);

    protected:
    };

	bool ZwIsFileExist(const String<WCHAR>& file_path_str);
	ull IoGetFileSize(const WCHAR* file_path);

	NTSTATUS ResolveSymbolicLink(const PUNICODE_STRING& link, const PUNICODE_STRING& resolved);
	NTSTATUS NormalizeDevicePath(const PCUNICODE_STRING& path, const PUNICODE_STRING& normalized);
	String<WCHAR> NormalizeDevicePathStr(const String<WCHAR>& path);

}

#endif // FILE_H
