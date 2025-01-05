#include <cstdint>
#include "../include/memory.h"

namespace ytd
{
    void* memset(void* s, const int c, size_t count) noexcept
    {
        auto* xs = static_cast<uint8_t*>(s);
        const auto b = static_cast<uint8_t>(c);
        if (count < 8)
        {
            while (count--)
                *xs++ = b;
            return s;
        }

        size_t align = -reinterpret_cast<uintptr_t>(xs) & (sizeof(size_t) - 1);
        count -= align;
        while (align--)
            *xs++ = b;

        size_t pattern = (static_cast<size_t>(b) << 24) | (static_cast<size_t>(b) << 16) | (static_cast<size_t>(b) << 8) | b;
        pattern = (pattern << 32) | pattern;

        auto* xw = reinterpret_cast<size_t*>(xs);
        while (count >= sizeof(size_t))
        {
            *xw++ = pattern;
            count -= sizeof(size_t);
        }

        xs = reinterpret_cast<uint8_t*>(xw);
        while (count--)
            *xs++ = b;

        return s;
    }

    void* memcpy(void* dest, const void* src, size_t count) noexcept
    {
        auto* d = static_cast<uint8_t*>(dest);
        const auto* s = static_cast<const uint8_t*>(src);
        if (count < 8)
        {
            while (count--)
                *d++ = *s++;
            return dest;
        }

        size_t align = -reinterpret_cast<uintptr_t>(d) & (sizeof(size_t) - 1);
        count -= align;
        while (align--)
            *d++ = *s++;

        auto* dw = reinterpret_cast<size_t*>(d);
        auto* sw = reinterpret_cast<const size_t*>(s);

        while (count >= sizeof(size_t))
        {
            *dw++ = *sw++;
            count -= sizeof(size_t);
        }

        d = reinterpret_cast<uint8_t*>(dw);
        s = reinterpret_cast<const uint8_t*>(sw);
        while (count--)
            *d++ = *s++;

        return dest;
    }

    int memcmp(const void* cs, const void* ct, size_t count) noexcept
    {
        const auto* s1 = static_cast<const uint8_t*>(cs);
        const auto* s2 = static_cast<const uint8_t*>(ct);
        if (count < 8)
        {
            while (count--)
            {
                if (*s1 != *s2)
                    return *s1 - *s2;
                s1++;
                s2++;
            }
            return 0;
        }

        size_t align = -reinterpret_cast<uintptr_t>(s1) & (sizeof(size_t) - 1);
        count -= align;
        while (align--)
        {
            if (*s1 != *s2)
                return *s1 - *s2;
            s1++;
            s2++;
        }

        const auto* w1 = reinterpret_cast<const size_t*>(s1);
        const auto* w2 = reinterpret_cast<const size_t*>(s2);
        while (count >= sizeof(size_t))
        {
            if (*w1 != *w2)
            {
                s1 = reinterpret_cast<const uint8_t*>(w1);
                s2 = reinterpret_cast<const uint8_t*>(w2);
                for (size_t i = 0; i < sizeof(size_t); i++)
                {
                    if (s1[i] != s2[i])
                        return s1[i] - s2[i];
                }
            }
            w1++;
            w2++;
            count -= sizeof(size_t);
        }

        s1 = reinterpret_cast<const uint8_t*>(w1);
        s2 = reinterpret_cast<const uint8_t*>(w2);
        while (count--)
        {
            if (*s1 != *s2)
                return *s1 - *s2;
            s1++;
            s2++;
        }

        return 0;
    }

    void* memmove(void* dest, const void* src, size_t count) noexcept
    {
        auto* d = static_cast<uint8_t*>(dest);
        const auto* s = static_cast<const uint8_t*>(src);

        if (d == s || count == 0)
            return dest;

        if (d > s && d < s + count)
        {
            d += count;
            s += count;

            if (count >= 8)
            {
                size_t align = reinterpret_cast<uintptr_t>(d) & (sizeof(size_t) - 1);
                count -= align;
                while (align--)
                    *--d = *--s;

                auto* dw = reinterpret_cast<size_t*>(d);
                auto* sw = reinterpret_cast<const size_t*>(s);

                while (count >= sizeof(size_t))
                {
                    *--dw = *--sw;
                    count -= sizeof(size_t);
                }

                d = reinterpret_cast<uint8_t*>(dw);
                s = reinterpret_cast<const uint8_t*>(sw);
            }

            while (count--)
                *--d = *--s;
        }
        else
        {
            if (count >= 8)
            {
                size_t align = -reinterpret_cast<uintptr_t>(d) & (sizeof(size_t) - 1);
                count -= align;
                while (align--)
                    *d++ = *s++;

                auto* dw = reinterpret_cast<size_t*>(d);
                auto* sw = reinterpret_cast<const size_t*>(s);

                while (count >= sizeof(size_t))
                {
                    *dw++ = *sw++;
                    count -= sizeof(size_t);
                }

                d = reinterpret_cast<uint8_t*>(dw);
                s = reinterpret_cast<const uint8_t*>(sw);
            }

            while (count--)
                *d++ = *s++;
        }

        return dest;
    }
}