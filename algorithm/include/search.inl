#pragma once

namespace ytl
{
    template<typename T>
    T lower_bound(const T *t, const size_t n, T x)
    {
        size_t k = 1;
        while (k <= n / 2)
            k = 2 * k + (1 & -(t[k] < x));
        k = 2 * k + (1 & -(k <= n && t[k] < x));
        k >>= __builtin_ctz(~k);
        return t[k];
    }

    template<typename T>
    T upper_bound(const T *t, const size_t n, T x)
    {
        size_t k = 1;
        while (k <= n / 2)
            k = 2 * k + (1 & -(t[k] <= x));
        k = 2 * k + (1 & -(k <= n && t[k] <= x));
        k >>= __builtin_ctz(~k);
        return t[k];
    }

    template<typename T>
    bool binary_search(const T *t, const size_t n, T x)
    {
        size_t k = 1;
        while (k <= n / 2)
            k = 2 * k + (1 & -(t[k] <= x));
        k = 2 * k + (1 & -(k <= n && t[k] <= x));
        k >>= __builtin_ctz(~k);
        return t[k] == x;
    }

    template<typename T>
    const T *find(const T *t, size_t n, const T &value)
    {
        for (size_t i = 0; i < n; ++i)
            if (t[i] == value)
                return &t[i];
        return &t[n];
    }

    template<typename T>
    size_t count(const T *t, size_t n, const T &value)
    {
        size_t count = 0;
        for (size_t i = 0; i < n; ++i)
            count += (t[i] == value);
        return count;
    }

    template<typename T>
    bool equal(const T *first1, const T *first2, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            if (!(first1[i] == first2[i]))
                return false;
        return true;
    }
}
