#include "memory.h"

void* operator new(ull n)
{
    void* p;
    while ((p = krnl_std::Alloc(n)) == 0);
    return (p);
}

void* operator new[](ull n)
{
    void* p;
    while ((p = krnl_std::Alloc(n)) == 0);
    return (p);
}

void operator delete(void* p, ull n)
{
    krnl_std::Free(p);
}

void operator delete[](void* p)
{
    krnl_std::Free(p);
}

void operator delete[](void* p, ull n)
{
    krnl_std::Free(p);
}

void SetUlongAt(ull addr, ULONG value)
{
    *(ULONG*)addr = value;
}

ULONG GetUlongAt(ull addr)
{
    return *(ULONG*)addr;
}

ull Rand()
{
    LARGE_INTEGER seed;
    KeQueryTickCount(&seed);
    seed.LowPart = (ULONG)(seed.LowPart ^ (ull)PsGetCurrentThreadId() ^ (ull)PsGetCurrentProcessId());
    seed.HighPart = (ULONG)(seed.HighPart ^ (ull)PsGetCurrentThreadId() ^ (ull)PsGetCurrentProcessId());
    seed.LowPart = seed.LowPart ^ (seed.LowPart << 13);
    seed.LowPart = seed.LowPart ^ (seed.LowPart >> 17);
    seed.LowPart = seed.LowPart ^ (seed.LowPart << 5);
    return seed.LowPart;
}