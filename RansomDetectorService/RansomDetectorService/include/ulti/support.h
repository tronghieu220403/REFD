#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef ETWSERVICE_ULTI_SUPPORT_H_
#define ETWSERVICE_ULTI_SUPPORT_H_

#include "include.h"

namespace ulti
{
    std::wstring StrToWstr(const std::string& str);
    std::string WstrToStr(const std::wstring& wstr);
    std::string CharVectorToString(const std::vector<char>& v);
    std::vector<char> StringToVectorChar(const std::string& s);
    std::vector<unsigned char> StringToVectorUChar(const std::string& s);
    std::wstring ToLower(const std::wstring& wstr);
    void ToLowerOverride(std::string& wstr);

    bool IsCurrentX86Process();

    bool CreateDir(const std::wstring& dir_path);

    bool KillProcess(DWORD pid);

    bool IsRunningAsSystem();

}

#endif
