#pragma once

namespace ytl
{
    template<typename T>
    T accumulate(const T *t, size_t n, T init)
    {
        for (size_t i = 0; i < n; ++i)
            init = init + t[i];
        return init;
    }

    template<typename T>
    void adjacent_difference(const T *t, T *result, size_t n)
    {
        if (n == 0)
            return;

        result[0] = t[0];
        for (size_t i = 1; i < n; ++i)
            result[i] = t[i] - t[i - 1];
    }
}
