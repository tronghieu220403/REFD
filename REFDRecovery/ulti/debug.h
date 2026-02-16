#pragma once

#ifndef ULTI_DEBUG_H_
#define ULTI_DEBUG_H_

#include "include.h"

#define DEBUG_LOG_THRESHOLD 1000
#define LOG_PATH L"C:\\hieunt_filetype.log"

namespace debug {

    // Logs a formatted debug message.
    void DebugPrintW(const wchar_t* pwsz_format, ...);

    // Retrieves a formatted error message from a Windows error code.
    std::wstring GetErrorMessage(DWORD errorCode);

}  // namespace debug 

#define PrintDebugW(str, ...) \
    debug::DebugPrintW(L"[%ws:%d] " str L"\n", __FUNCTIONW__, __LINE__, __VA_ARGS__)

#endif // ULTI_DEBUG_H_