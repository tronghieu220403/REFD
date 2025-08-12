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

