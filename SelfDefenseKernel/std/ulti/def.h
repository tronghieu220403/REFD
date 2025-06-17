#pragma once
typedef long long ptrdiff_t;

#pragma warning(disable:4100)

template <typename T>
struct Less {
public:
    inline bool operator()(const T& lhs, const T& rhs) const {
        return lhs < rhs;
    }
};

template <typename T>
struct Greater {
public:
    inline bool operator()(const T& lhs, const T& rhs) const {
        return lhs > rhs;
    }
};
