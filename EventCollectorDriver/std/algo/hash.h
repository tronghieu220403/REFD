#ifndef HASH_H
#define HASH_H

#include "../string/wstring.h"

inline unsigned long long HashWstring(const std::WString& str)
{
    unsigned long long hash = 5381; // Magic number 5381 is used in DJB2 hash function
    for (int i = 0; i < str.Size(); i++) {
        WCHAR c = str[i];
        hash = ((hash << 5) + hash) + static_cast<unsigned long long>(c); // hash * 33 + c
    }
    return hash;
}

inline unsigned long long HashString(const WString& str)
{
    unsigned long long hash = 5381; // Magic number 5381 is used in DJB2 hash function
    for (int i = 0; i < str.Size(); i++) {
        WCHAR c = str[i];
        hash = ((hash << 5) + hash) + static_cast<unsigned long long>(c); // hash * 33 + c
    }
    return hash;
}

#endif