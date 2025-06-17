#ifndef SELF_DEFENSE_H
#define SELF_DEFENSE_H

#include <fltKernel.h>
#include "../../std/string/string.h"
#include "../../std/vector/vector.h"

namespace self_defense {

    // Khai báo các hàm đăng ký và huỷ đăng ký
    void DrvRegister();
    void DrvUnload();
    void FltRegister();
	NTSTATUS FltUnload();

    // Các callback bảo vệ
    FLT_PREOP_CALLBACK_STATUS PreCreateFile(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);
    FLT_PREOP_CALLBACK_STATUS PreSetInformationFile(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);
    OB_PREOP_CALLBACK_STATUS PreObCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation);

    // Process notification callback
    VOID ProcessNotifyCallback(
        PEPROCESS Process,
        HANDLE ProcessId,
        PPS_CREATE_NOTIFY_INFO CreateInfo
    );

    // Các hàm trợ giúp
    bool IsProtectedFile(const String<WCHAR>& path);
    bool IsInProtectedFile(const String<WCHAR>& path);
    bool IsProtectedProcess(HANDLE pid);

    String<WCHAR> GetProcessImageName(HANDLE pid);
	Vector<String<WCHAR>> GetDefaultProtectedDirs();
	Vector<String<WCHAR>> GetDefaultProtectedFiles();

} // namespace self_defense

#endif // SELF_DEFENSE_H
