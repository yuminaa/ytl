#pragma once

#include <cstddef>

namespace ytd
{
    size_t strlen(const char* s) noexcept;
    constexpr size_t strnlen(const char* s, size_t maxlen) noexcept;

    char* strcpy(char* dest, const char* src) noexcept;
    char* strncpy(char* dest, const char* src, size_t count) noexcept;
    size_t strlcpy(char* dest, const char* src, size_t size) noexcept;

    char* strcat(char* dest, const char* src) noexcept;
    char* strncat(char* dest, const char* src, size_t count) noexcept;
    size_t strlcat(char* dest, const char* src, size_t size) noexcept;

    int strcmp(const char* s1, const char* s2) noexcept;
    constexpr int strncmp(const char* s1, const char* s2, size_t count) noexcept;
    constexpr int strcasecmp(const char* s1, const char* s2) noexcept;
    constexpr int strncasecmp(const char* s1, const char* s2, size_t count) noexcept;

    const char* strchr(const char* s, int ch) noexcept;
    const char* strrchr(const char* s, int ch) noexcept;
    const char* strnchr(const char* s, size_t count, int ch) noexcept;
    const char* strstr(const char* haystack, const char* needle) noexcept;
    const char* strnstr(const char* haystack, const char* needle, size_t len) noexcept;

    char* strchr(char* s, int ch) noexcept;
    char* strrchr(char* s, int ch) noexcept;
    char* strnchr(char* s, size_t count, int ch) noexcept;
    char* strstr(char* haystack, const char* needle) noexcept;
    char* strnstr(char* haystack, const char* needle, size_t len) noexcept;
}