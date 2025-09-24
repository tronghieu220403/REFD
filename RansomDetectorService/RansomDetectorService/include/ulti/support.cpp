#include "support.h"
#include "debug.h"

namespace ulti
{
    std::wstring StrToWstr(const std::string& str) {
        if (str.empty()) return L"";

        int size_needed = MultiByteToWideChar(CP_UTF8, 0,
            str.c_str(), static_cast<int>(str.size()),
            nullptr, 0);
        if (size_needed <= 0) {
            return L"";
        }

        std::wstring wstr(size_needed, L'\0');
        MultiByteToWideChar(CP_UTF8, 0,
            str.c_str(), static_cast<int>(str.size()),
            &wstr[0], size_needed);
        return wstr;
    }

    std::string WstrToStr(const std::wstring& wstr) {
        if (wstr.empty()) return "";

        int size_needed = WideCharToMultiByte(CP_UTF8, 0,
            wstr.c_str(), static_cast<int>(wstr.size()),
            nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) {
            return "";
        }

        std::string str(size_needed, '\0');
        WideCharToMultiByte(CP_UTF8, 0, 
            wstr.c_str(), static_cast<int>(wstr.size()), 
            &str[0], size_needed, nullptr, nullptr);
        return str;
    }

    std::string CharVectorToString(const std::vector<char>& v)
    {
        return std::string(v.begin(), v.end());
    }

    std::vector<char> StringToVectorChar(const std::string& s)
    {
        return std::vector<char>(s.begin(), s.end());
    }

    std::vector<unsigned char> StringToVectorUChar(const std::string& s)
    {
        return std::vector<unsigned char>(s.begin(), s.end());
    }

    std::wstring ToLower(const std::wstring& wstr)
    {
        std::wstring result = wstr;
        CharLowerBuffW(result.data(), result.size());
        return result;
    }

    std::string ToLower(const std::string& wstr)
    {
        std::string result = wstr;
        CharLowerBuffA(result.data(), result.size());
        return result;
    }

    void ToLowerOverride(std::wstring& wstr)
    {
        CharLowerBuffW(wstr.data(), wstr.size());
    }

    void ToLowerOverride(std::string& wstr)
    {
        CharLowerBuffA(wstr.data(), wstr.size());
    }

    bool IsCurrentX86Process()
    {
        #if defined(__x86_64__) || defined(_M_X64)
            return false;
        #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
            return true;
        #endif
    }

    bool CreateDir(const std::wstring& dir_path)
    {
        BOOL status = ::CreateDirectory(dir_path.c_str(), NULL);
        if (status == TRUE || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            return true;
        }
        else if (GetLastError() == ERROR_PATH_NOT_FOUND)
        {
            try {
                fs::create_directories(dir_path);
            }
            catch (const fs::filesystem_error&) {
                return false;
            }
            return true;
        }
        else {
            return false;
        }

        return false;
    }
    
    bool KillProcess(DWORD pid)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) {
            return false;
        }
        defer{ CloseHandle(hProcess); };
        if (!TerminateProcess(hProcess, 0)) {
            return false;
        }
        return true;
    }

    bool IsRunningAsSystem() {
        HANDLE h_token = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h_token)) {
            return false;
        }

        DWORD size = 0;
        TOKEN_USER* token_user = nullptr;
        GetTokenInformation(h_token, TokenUser, NULL, 0, &size);
        token_user = (TOKEN_USER*)malloc(size);

        if (!token_user || !GetTokenInformation(h_token, TokenUser, token_user, size, &size)) {
            if (h_token) CloseHandle(h_token);
            if (token_user) free(token_user);
            return false;
        }

        SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
        PSID system_sid = NULL;
        if (!AllocateAndInitializeSid(&nt_authority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &system_sid)) {
            if (h_token) CloseHandle(h_token);
            if (token_user) free(token_user);
            return false;
        }

        BOOL is_system = EqualSid(system_sid, token_user->User.Sid);
        FreeSid(system_sid);
        CloseHandle(h_token);
        free(token_user);

        return is_system;
    }

    std::wstring GetProcessPath(DWORD pid) {
        std::wstring path;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            WCHAR buffer[MAX_PATH];
            DWORD size = MAX_PATH;

            if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
                path.assign(buffer, size);
            }

            CloseHandle(hProcess);
        }

        return path;
    }
}
