#pragma once

namespace ytd
{
    namespace detail
    {
        template<typename T>
        void sift_up(T* t, size_t pos)
        {
            while (pos > 0) {
                size_t parent = (pos - 1) / 2;
                if (t[parent] >= t[pos])
                    break;
                swap(t[parent], t[pos]);
                pos = parent;
            }
        }

        template<typename T>
        void sift_down(T* t, size_t pos, size_t n)
        {
            while (true)
            {
                size_t max_idx = pos;
                size_t left = 2 * pos + 1;
                size_t right = 2 * pos + 2;

                if (left < n && t[left] > t[max_idx])
                    max_idx = left;
                if (right < n && t[right] > t[max_idx])
                    max_idx = right;

                if (max_idx == pos)
                    break;

                swap(t[pos], t[max_idx]);
                pos = max_idx;
            }
        }
    }

    template<typename T>
    void make_heap(T* t, size_t n)
    {
        for (size_t i = n/2; i > 0; --i)
            detail::sift_down(t, i-1, n);
    }

    template<typename T>
    void push_heap(T* t, size_t n)
    {
        detail::sift_up(t, n-1);
    }

    template<typename T>
    void pop_heap(T* t, size_t n)
    {
        if (n <= 1) return;
        swap(t[0], t[n-1]);
        detail::sift_down(t, 0, n-1);
    }

    template<typename T>
    void sort_heap(T* t, size_t n)
    {
        for (size_t i = n; i > 1; --i) {
            swap(t[0], t[i-1]);
            detail::sift_down(t, 0, i-1);
        }
    }
}