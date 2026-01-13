#pragma once
typedef long long ptrdiff_t;

#pragma warning(disable:4100)

template <typename ValType>
struct Less {
public:
    inline bool operator()(const ValType& lhs, const ValType& rhs) const {
        return lhs < rhs;
    }
};

template <typename ValType>
struct Greater {
public:
    inline bool operator()(const ValType& lhs, const ValType& rhs) const {
        return lhs > rhs;
    }
};

template <class T> struct remove_reference { using type = T; };
template <class T> struct remove_reference<T&> { using type = T; };
template <class T> struct remove_reference<T&&> { using type = T; };
template <class T> using remove_reference_t = typename remove_reference<T>::type;

template <class T>
constexpr remove_reference_t<T>&& Move(T&& t) noexcept {
	return static_cast<remove_reference_t<T>&&>(t);
}

/// RAII wrapper for callable statements invoked by DEFER()
template <typename F>
struct ScopedDefer
{
    ScopedDefer(F f)
        : f(f)
    {
    }

    ~ScopedDefer() { f(); }

    F f;
};

/// A common macro for string concatenation during preprocessing phase.
#define STR_CONCAT(a, b) _STR_CONCAT(a, b)
#define _STR_CONCAT(a, b) a##b

/// Implementation of "defer" keyword similar in Go and Zig to automatically
/// call resource cleanup at end of function scope without copy-pasted cleanup
/// statements or separate RAII wrapper data types.
#define defer(x) const auto STR_CONCAT(tmpDeferVarName, __LINE__) = ScopedDefer([&]() { x; })
