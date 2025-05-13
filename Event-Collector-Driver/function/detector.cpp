#include "detector.h"

namespace detector
{
    bool IsExtensionMonitored(const WCHAR* p_file_path)
    {
        auto file_path_len = wcslen(p_file_path);
        for (const auto& ext : kMonitoredExtension)
        {
            bool match = true;
            auto ext_len = wcslen(ext);
            if (file_path_len < ext_len)
            {
                continue;
            }
            for (ull i = 0; i < ext_len; ++i)
            {
                WCHAR c = p_file_path[file_path_len - ext_len + i];
                if (L'A' <= c && c <= L'Z')
                {
                    c = c - L'A' + L'a';
                }
                if (c != ext[i])
                {
                    match = false;
                    break;
                }
            }
            if (match == true)
            {
                return true;
            }
        }
        return false;
    }

    bool IsSameFileExtension(const WCHAR* p_file_path_1, const WCHAR* p_file_path_2)
    {
        auto file_path_1_len = wcslen(p_file_path_1);
        auto file_path_2_len = wcslen(p_file_path_2);

        bool ans = true;
        for (int i = 0; ; ++i)
        {
            if (i >= file_path_1_len || i >= file_path_2_len)
            {
                ans = false;
                break;
            }
            WCHAR c1 = p_file_path_1[file_path_1_len - i - 1];
            if (L'A' <= c1 && c1 <= L'Z')
            {
                c1 = c1 - L'A' + L'a';
            }
            WCHAR c2 = p_file_path_2[file_path_2_len - i - 1];
            if (L'A' <= c2 && c2 <= L'Z')
            {
                c2 = c2 - L'A' + L'a';
            }
            if (c1 != c2)
            {
                ans = false;
                break;
            }
            if (c1 == L'.')
            {
                break;
            }
            if (c1 == L'\\' || c1 == L'/')
            {
                ans = false;
                break;
            }
        }

        return ans;
    }
}