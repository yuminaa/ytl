#pragma once

namespace ytd
{
    namespace detail
    {
        template<typename T>
        size_t partition(T *t, const size_t n)
        {
            T pivot = t[n - 1];
            size_t i = 0;

            for (size_t j = 0; j < n - 1; ++j)
            {
                if (t[j] <= pivot)
                {
                    swap(t[i], t[j]);
                    ++i;
                }
            }

            swap(t[i], t[n - 1]);
            return i;
        }

        template<typename T>
        void quick_sort(T *t, size_t n)
        {
            if (n <= 1)
                return;

            size_t p = partition(t, n);
            quick_sort(t, p);
            quick_sort(t + p + 1, n - p - 1);
        }

        template<typename T>
        void sift_down(T *t, const size_t start, const size_t end)
        {
            size_t root = start;

            while (root * 2 + 1 <= end)
            {
                size_t child = root * 2 + 1;
                size_t swap_idx = root;

                if (t[swap_idx] < t[child])
                    swap_idx = child;

                if (child + 1 <= end && t[swap_idx] < t[child + 1])
                    swap_idx = child + 1;

                if (swap_idx == root)
                    return;

                swap(t[root], t[swap_idx]);
                root = swap_idx;
            }
        }
    }

    template<typename T>
    void sort(T *t, size_t n)
    {
        detail::quick_sort(t, n);
    }

    template<typename T>
    void partial_sort(T *t, size_t n, size_t m)
    {
        if (m >= n)
        {
            sort(t, n);
            return;
        }

        for (size_t i = (m - 1) / 2; i != static_cast<size_t>(-1); --i)
            detail::sift_down(t, i, m - 1);

        for (size_t i = m; i < n; ++i)
        {
            if (t[i] < t[0])
            {
                swap(t[0], t[i]);
                detail::sift_down(t, 0, m - 1);
            }
        }

        for (size_t i = m - 1; i > 0; --i)
        {
            swap(t[0], t[i]);
            detail::sift_down(t, 0, i - 1);
        }
    }

    template<typename T>
    void nth_element(T *t, const size_t n, const size_t nth)
    {
        if (nth >= n)
            return;

        size_t left = 0;
        size_t right = n - 1;

        while (true)
        {
            size_t p = detail::partition(t + left, right - left + 1);
            p += left;

            if (p == nth)
                return;
            if (p < nth)
                left = p + 1;
            else
                right = p - 1;
        }
    }
}
