#pragma once
#define _CRT_SECURE_NO_DEPRECATE
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#ifndef ETWSERVICE_ULTI_INCLUDE_H_
#define ETWSERVICE_ULTI_INCLUDE_H_
#include <Windows.h>
#include <iphlpapi.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntcons.h>
#include <comutil.h>
#include <wbemidl.h>
#include <winternl.h>
#include <wtsapi32.h>
#include <userenv.h>

#include <memory>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <functional>
#include <chrono>
#include <deque>
#include <syncstream>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <list>
#include <regex>
#include <array>
#include <locale>
#include <cctype>
#include <codecvt>
#include <cwctype>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "FltLib.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Kernel32.lib")

template<typename F>
class defer_finalizer {
    F f;
    bool moved;
public:
    template<typename T>
    defer_finalizer(T&& f_) : f(std::forward<T>(f_)), moved(false) { }

    defer_finalizer(const defer_finalizer&) = delete;

    defer_finalizer(defer_finalizer&& other) : f(std::move(other.f)), moved(other.moved) {
        other.moved = true;
    }

    ~defer_finalizer() {
        if (!moved) f();
    }
};

struct {
    template<typename F>
    defer_finalizer<F> operator<<(F&& f) {
        return defer_finalizer<F>(std::forward<F>(f));
    }
} deferrer;

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define defer auto TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = deferrer << [&]

#pragma warning(disable : 26110)

typedef unsigned int uint32;

typedef unsigned long long ull;

#define FlagOn(_Flag, _Bit)  (((_Flag) & (_Bit)) != 0)

#define ZeroMem(data,size) ZeroMemory(data,size)

#define GetCurrentDir _getcwd

namespace fs = std::filesystem;

using namespace std;

#endif // ETWSERVICE_ULTI_INCLUDE_H_