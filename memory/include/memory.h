#pragma once
#include <cstddef>

namespace ytl
{
    void* memset(void* s, int c, size_t count) noexcept;
    void* memcpy(void* dest, const void* src, size_t count) noexcept;
    void* memmove(void* dest, const void* src, size_t count) noexcept;
    int memcmp(const void* cs, const void* ct, size_t count) noexcept;
}