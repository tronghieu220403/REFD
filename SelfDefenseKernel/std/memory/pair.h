#pragma once

#include "memory.h"

// Pair structure
template <class T1, class T2>
struct Pair {
public:
    T1 first;
    T2 second;

    Pair() : first(), second() {}
    Pair(const T1& first, const T2& second) : first(first), second(second) {}

    Pair(const Pair& p) = default;
    Pair(Pair&& p) = default;

    Pair& operator=(const Pair& other) {
        first = other.first;
        second = other.second;
        return *this;
    }

    bool operator==(const Pair& other) const {
        return first == other.first && second == other.second;
    }

    bool operator!=(const Pair& other) const {
        return !(*this == other);
    }

    bool operator<(const Pair& other) const {
        return first < other.first ||
            (!(other.first < first) && second < other.second);
    }

    bool operator>(const Pair& other) const {
        return other < *this;
    }

    bool operator<=(const Pair& other) const {
        return !(other < *this);
    }

    bool operator>=(const Pair& other) const {
        return !(*this < other);
    }
};

// MakePair function
template <class T1, class T2>
Pair<T1, T2> MakePair(const T1& a, const T2& b) {
    return Pair<T1, T2>(a, b);
}
