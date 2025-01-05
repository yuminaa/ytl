#pragma once

namespace ytl
{
    template<typename T>
    void copy(const T *src, T *dest, size_t n)
    {
        if (dest > src && dest < src + n)
        {
            for (size_t i = n; i > 0; --i)
                dest[i - 1] = src[i - 1];
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
                dest[i] = src[i];
        }
    }

    template<typename T>
    void fill(T *t, size_t n, const T &value)
    {
        for (size_t i = 0; i < n; ++i)
            t[i] = value;
    }

    template<typename T>
    void reverse(T *t, size_t n)
    {
        for (size_t i = 0; i < n / 2; ++i)
            swap(t[i], t[n - 1 - i]);
    }

    template<typename T>
    T *rotate(T *t, size_t n, size_t mid)
    {
        if (mid == 0 || mid == n)
            return t + mid;

        reverse(t, mid);
        reverse(t + mid, n - mid);
        reverse(t, n);

        return t + mid;
    }
}
