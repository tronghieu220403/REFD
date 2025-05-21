#include "support.h"
#include "debug.h"

namespace ulti
{
    std::wstring StrToWstr(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
        return myconv.from_bytes(str);
    }

    std::string WstrToStr(const std::wstring& wstr)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> myconv;
        return myconv.to_bytes(wstr);
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
        std::setlocale(LC_ALL, "en_US.utf8");
        defer{
            std::setlocale(LC_ALL, nullptr);
        };
        std::wstring result = wstr;
        for (wchar_t& c : result)
        {
            if (std::iswalpha(c))
            {
                c = std::towlower(c);
            }
        }
        return result;
    }

    void ToLowerOverride(std::string& wstr)
    {
        std::setlocale(LC_ALL, "en_US.utf8");
        defer{
            std::setlocale(LC_ALL, nullptr);
        };
        for (char& c : wstr)
        {
            if (std::isalpha(c))
            {
                c = std::tolower(c);
            }
        }
    }

    bool IsCurrentX86Process()
    {
        static bool is_eval = false;
        static bool is_x86 = false;
        if (is_eval)
        {
            return is_x86;
        }
        SYSTEM_INFO systemInfo = { 0 };
        GetNativeSystemInfo(&systemInfo);

        // x86 environment
        if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        {
            is_x86 = true;
            is_eval = true;
            return true;
        }

        BOOL is_wow64 = FALSE;
        if (IsWow64Process(GetCurrentProcess(), &is_wow64) == FALSE)
        {
            debug::DebugPrintW(L"IsWow64Process error 0x%x", GetLastError());
        }
        is_x86 = (is_wow64 == FALSE) ? false : true;
        is_eval = true;
        return is_wow64;
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

}
