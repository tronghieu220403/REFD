#include "common.h"

std::WString GetProcessImageName(HANDLE pid)
{
    std::WString process_image_name;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PEPROCESS eproc;
    status = PsLookupProcessByProcessId(pid, &eproc);
    defer(ObDereferenceObject(eproc););
    if (!NT_SUCCESS(status))
    {
        //DebugMessage("PsLookupProcessByProcessId for pid %p Failed: %08x\n", pid, status);
        return std::WString();
    }
    PUNICODE_STRING process_name = NULL;
    status = SeLocateProcessImageName(eproc, &process_name);
    if (status == STATUS_SUCCESS)
    {
        process_image_name = process_name;
        return process_image_name;
    }

    /*
    ULONG returned_length = 0;
    HANDLE h_process = NULL;

    Vector<UCHAR> buffer;

    if (ZwQueryInformationProcess == NULL)
    {
        //DebugMessage("Cannot resolve ZwQueryInformationProcess\n");
        status = STATUS_UNSUCCESSFUL;
        return process_image_name;
    }

    status = ObOpenObjectByPointer(eproc,
        0, NULL, 0, 0, KernelMode, &h_process);
    if (!NT_SUCCESS(status))
    {
        //DebugMessage("ObOpenObjectByPointer Failed: %08x\n", status);
        return process_image_name;
    }
    defer(if (h_process) ZwClose(h_process););

    status = ZwQueryInformationProcess(h_process,
        ProcessImageFileName,
        NULL,
        0,
        &returned_length);

    if (STATUS_INFO_LENGTH_MISMATCH != status) {
        //DebugMessage("ZwQueryInformationProcess status = %x\n", status);
        return process_image_name;
    }

    buffer.Resize(returned_length);

    if (buffer.Data() == NULL)
    {
        return process_image_name;
    }

    status = ZwQueryInformationProcess(h_process,
        ProcessImageFileName,
        buffer.Data(),
        returned_length,
        &returned_length);
    process_image_name = (PUNICODE_STRING)(buffer.Data());
    */
    //DebugMessage("GetProcessImageName: %ws", process_image_name.Data());
    return process_image_name;
}

static __forceinline LONGLONG GetNtSystemTime()
{
    LARGE_INTEGER systemTime;
    KeQuerySystemTime(&systemTime);
    return systemTime.QuadPart; // 100-ns since 1601
}