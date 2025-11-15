#pragma once
#ifndef TRID_TRID_H_
#define TRID_TRID_H_
#ifdef _M_IX86
#include "../ulti/include.h"
#include "trid_api.h"

namespace type_iden
{
    // TrID lib can only run on x86 process. Do not use this in async multi-thread.
    // To use TrID on async multi-thread, each instance of TrID must create and load an unique clone of TrIDLib.dll (for example TrIDLib_X.dll)
    class TrID
    {
    private:
        std::mutex m_mutex_;
        TridApi* trid_api_ = nullptr;
    public:
        TrID() = default;
        ~TrID();
        bool Init(const std::wstring& defs_dir, const std::wstring& trid_dll_path);
        std::vector<std::string> GetTypes(const std::wstring& file_path);
    };
}
#endif // _M_IX86
#endif // RDS_TRID_TRID_H_
