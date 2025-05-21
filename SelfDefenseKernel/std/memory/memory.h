#pragma once

typedef unsigned long long ull;

#include <ntifs.h>
#include <ntdef.h>
#include <wdm.h>
#include "../ulti/def.h"
#include "../../template/debug.h"

#pragma warning(disable:4100)

#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

#define INT_MAX 2147483647
#define ULL_MAX 0xFFFFFFFFFFFFFFFF

extern void* operator new(ull);
extern void* operator new[](ull);

extern void operator delete(void*, ull);

extern void operator delete[](void*);

template <class T>
void MemCopy(T* dst, T* src, ull len);

template <class T>
void ZeroMemory(T* dst, ull len);

ull Rand();

namespace krnl_std
{
    void* Alloc(ull n);

    void Free(void* p);
}

template <class T>
inline void MemCopy(T* dst, T* src, ull len)
{
    RtlCopyMemory(dst, src, len * sizeof(T));
    return;
}

template <class T>
inline void ZeroMemory(T* dst, ull len)
{
    for (int i = 0; i < len; ++i)
    {
        dst[i] = T();
    }
}

void SetUlongAt(ull addr, ULONG value);
ULONG GetUlongAt(ull addr);

namespace krnl_std
{
    inline void* Alloc(ull n)
    {
        void* p = nullptr;
        p = ExAllocatePool2(POOL_FLAG_NON_PAGED, n, 0x22042003); // Windows 10 2004 above
        //p = ExAllocatePoolWithTag(NonPagedPool, n, 0x22042003);
        return p;
    }

    inline void Free(void* p)
    {
        if (p == nullptr)
        {
            return;
        }
        ExFreePoolWithTag(p, 0x22042003);
    }

}
