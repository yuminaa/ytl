#include "../include/string.h"

namespace ytd::detail
{
    static constexpr size_t ones = ~size_t { 0 } / 0xFF;
    static constexpr size_t highs = ones * 0x80;

    constexpr bool has_zero(const size_t x) noexcept
    {
        return ((x - ones) & ~x & highs) != 0;
    }
}

namespace ytd
{
    size_t strlen(const char *s) noexcept
    {
        const char *str = s;
        if (reinterpret_cast<size_t>(str) & (sizeof(size_t) - 1))
        {
            while (reinterpret_cast<size_t>(str) & (sizeof(size_t) - 1))
            {
                if (!*str)
                    return str - s;
                str++;
            }
        }

        const auto *ls = reinterpret_cast<const size_t *>(str);
        while (!detail::has_zero(*ls))
            ls++;

        str = reinterpret_cast<const char *>(ls);
        while (*str)
            str++;

        return str - s;
    }

    constexpr size_t strnlen(const char *s, size_t maxlen) noexcept
    {
        const char *es = s;
        while (*es && maxlen)
        {
            es++;
            maxlen--;
        }
        return es - s;
    }

    char *strcpy(char *dest, const char *src) noexcept
    {
        char *ret = dest;

        while (reinterpret_cast<size_t>(src) & (sizeof(size_t) - 1))
        {
            if ((*dest = *src) == '\0')
                return ret;
            dest++;
            src++;
        }

        auto *ldest = reinterpret_cast<size_t *>(dest);
        const auto *lsrc = reinterpret_cast<const size_t *>(src);
        while (!detail::has_zero(*lsrc))
            *ldest++ = *lsrc++;

        dest = reinterpret_cast<char *>(ldest);
        src = reinterpret_cast<const char *>(lsrc);
        while ((*dest++ = *src++) != '\0') {}

        return ret;
    }

    char *strncpy(char *dest, const char *src, size_t count) noexcept
    {
        char *d = dest;
        while (count && (*d = *src) != '\0')
        {
            d++;
            src++;
            count--;
        }
        while (count--)
            *d++ = 0;
        return dest;
    }

    size_t strlcpy(char *dest, const char *src, const size_t size) noexcept
    {
        const char *s = src;
        size_t n = size;
        if (n != 0 && --n != 0)
        {
            do
            {
                if ((*dest++ = *s++) == 0)
                    break;
            }
            while (--n != 0);
        }

        if (n == 0)
        {
            if (size != 0)
                *dest = '\0';
            while (*s++);
        }

        return s - src - 1;
    }

    char *strcat(char *dest, const char *src) noexcept
    {
        char *tmp = dest;
        while (*tmp)
            tmp++;

        while ((*tmp++ = *src++) != '\0') {}
        return dest;
    }

    char *strncat(char *dest, const char *src, size_t n) noexcept
    {
        char *tmp = dest;
        while (*tmp)
            tmp++;

        while (n-- > 0 && (*tmp = *src) != '\0')
        {
            tmp++;
            src++;
        }

        *tmp = '\0';
        return dest;
    }

    size_t strlcat(char *dest, const char *src, const size_t size) noexcept
    {
        const char *s = src;
        char *d = dest;
        size_t n = size;
        while (n-- != 0 && *d != '\0')
            d++;
        const size_t dlen = d - dest;
        n = size - dlen;

        if (n == 0)
            return dlen + strlen(s);

        while (*s != '\0')
        {
            if (n != 1)
            {
                *d++ = *s;
                n--;
            }
            s++;
        }
        *d = '\0';

        return dlen + (s - src);
    }

    int strcmp(const char *s1, const char *s2) noexcept
    {
        while (reinterpret_cast<size_t>(s1) & (sizeof(size_t) - 1))
        {
            if (*s1 != *s2)
                return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
            if (!*s1)
                return 0;
            s1++;
            s2++;
        }

        const auto *ls1 = reinterpret_cast<const size_t *>(s1);
        const auto *ls2 = reinterpret_cast<const size_t *>(s2);
        while (!detail::has_zero(*ls1) && *ls1 == *ls2)
        {
            ls1++;
            ls2++;
        }

        s1 = reinterpret_cast<const char *>(ls1);
        s2 = reinterpret_cast<const char *>(ls2);
        while (*s1 && *s1 == *s2)
        {
            s1++;
            s2++;
        }
        return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
    }

    constexpr int strncmp(const char *s1, const char *s2, size_t count) noexcept
    {
        while (count--)
        {
            const auto c1 = static_cast<unsigned char>(*s1++);
            if (auto c2 = static_cast<unsigned char>(*s2++);
                c1 != c2)
                return c1 - c2;
            if (!c1)
                break;
        }
        return 0;
    }

    constexpr int strcasecmp(const char *s1, const char *s2) noexcept
    {
        while (true)
        {
            const auto c1 = static_cast<unsigned char>(*s1++ | 0x20);
            if (auto c2 = static_cast<unsigned char>(*s2++ | 0x20);
                c1 != c2 || !c1)
                return c1 - c2;
        }
    }

    constexpr int strncasecmp(const char *s1, const char *s2, size_t len) noexcept
    {
        if (!len)
            return 0;

        do
        {
            const auto c1 = static_cast<unsigned char>(*s1++ | 0x20);
            if (const auto c2 = static_cast<unsigned char>(*s2++ | 0x20);
                !--len || c1 != c2)
                return c1 - c2;
        }
        while (true);
    }

    const char *strchr(const char *s, int c) noexcept
    {
        const auto ch = static_cast<unsigned char>(c);
        if (!ch)
        {
            while (*s)
                s++;
            return s;
        }

        while (*s && *s != ch)
            s++;
        return *s == ch ? s : nullptr;
    }

    char *strchr(char *s, int c) noexcept
    {
        return const_cast<char *>(strchr(static_cast<const char *>(s), c));
    }

    const char *strrchr(const char *s, int c) noexcept
    {
        const char *last = nullptr;
        do
        {
            if (*s == static_cast<char>(c))
                last = s;
        }
        while (*s++);
        return last;
    }

    char *strrchr(char *s, int c) noexcept
    {
        return const_cast<char *>(strrchr(static_cast<const char *>(s), c));
    }

    const char *strnchr(const char *s, size_t count, int c) noexcept
    {
        while (count--)
        {
            if (*s == static_cast<char>(c))
                return s;
            if (*s++ == '\0')
                break;
        }
        return nullptr;
    }

    char *strnchr(char *s, size_t count, int c) noexcept
    {
        return const_cast<char *>(strnchr(static_cast<const char *>(s), count, c));
    }

    const char *strnstr(const char *s1, const char *s2, size_t len) noexcept
    {
        const size_t l2 = strlen(s2);
        if (!l2)
            return s1;

        while (len >= l2)
        {
            if (!strncmp(s1, s2, l2))
                return s1;
            s1++;
            len--;
        }
        return nullptr;
    }

    char *strnstr(char *s1, const char *s2, size_t len) noexcept
    {
        return const_cast<char *>(strnstr(static_cast<const char *>(s1), s2, len));
    }

    const char *strstr(const char *haystack, const char *needle) noexcept
    {
        const char first = *needle;
        if (!first)
            return haystack;

        while (*haystack)
        {
            if (*haystack == first)
            {
                const char *h = haystack + 1;
                const char *n = needle + 1;

                while (*n && *h == *n)
                {
                    h++;
                    n++;
                }

                if (!*n)
                    return haystack;
            }
            haystack++;
        }

        return nullptr;
    }

    char *strstr(char *haystack, const char *needle) noexcept
    {
        return const_cast<char *>(strstr(static_cast<const char *>(haystack), needle));
    }
}
