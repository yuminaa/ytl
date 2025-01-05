#pragma once

namespace ytl
{
    template<typename T>
    const T &min(const T &a, const T &b)
    {
        return b < a ? b : a;
    }

    template<typename T>
    const T &max(const T &a, const T &b)
    {
        return a < b ? b : a;
    }

    template<typename T>
    bool lexicographical_compare(const T *t1, size_t n1, const T *t2, size_t n2)
    {
        size_t n = min(n1, n2);
        for (size_t i = 0; i < n; ++i)
        {
            if (t1[i] < t2[i])
                return true;
            if (t2[i] < t1[i])
                return false;
        }
        return n1 < n2;
    }

    template<typename T>
    void swap(T &a, T &b) noexcept
    {
        T tmp = static_cast<T &&>(a);
        a = static_cast<T &&>(b);
        b = static_cast<T &&>(tmp);
    }
}
