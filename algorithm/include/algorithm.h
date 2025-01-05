#pragma once

namespace ytl
{
    template<typename T>
    T lower_bound(const T *t, size_t n, T x);

    template<typename T>
    T upper_bound(const T *t, size_t n, T x);

    template<typename T>
    bool binary_search(const T *t, size_t n, T x);

    template<typename T>
    const T *find(const T *t, size_t n, const T &value);

    template<typename T>
    size_t count(const T *t, size_t n, const T &value);

    template<typename T>
    bool equal(const T *first1, const T *first2, size_t n);

    template<typename T>
    void copy(const T *src, T *dest, size_t n);

    template<typename T>
    void fill(T *t, size_t n, const T &value);

    template<typename T>
    void reverse(T *t, size_t n);

    template<typename T>
    T *rotate(T *t, size_t n, size_t mid);

    template<typename T>
    void sort(T *t, size_t n);

    template<typename T>
    void partial_sort(T *t, size_t n, size_t m);

    template<typename T>
    void nth_element(T *t, size_t n, size_t nth);

    template<typename T>
    const T &min(const T &a, const T &b);

    template<typename T>
    const T &max(const T &a, const T &b);

    template<typename T>
    bool lexicographical_compare(const T *t1, size_t n1, const T *t2, size_t n2);

    template<typename T>
    void swap(T &a, T &b) noexcept;

    template<typename T>
    void make_heap(T *t, size_t n);

    template<typename T>
    void push_heap(T *t, size_t n);

    template<typename T>
    void pop_heap(T *t, size_t n);

    template<typename T>
    void sort_heap(T *t, size_t n);

    template<typename T>
    T accumulate(const T *t, size_t n, T init);

    template<typename T>
    void adjacent_difference(const T *t, T *result, size_t n);
}

#include "compare.inl"
#include "heap.inl"
#include "numeric.inl"
#include "search.inl"
#include "sequence.inl"
#include "sort.inl"
