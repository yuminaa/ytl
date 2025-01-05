#pragma once

#include <array>
#include <cstddef>

namespace ytl
{
    void* memset(void* s, int c, size_t count) noexcept;
    void* memcpy(void* dest, const void* src, size_t count) noexcept;
    void* memmove(void* dest, const void* src, size_t count) noexcept;
    int memcmp(const void* cs, const void* ct, size_t count) noexcept;

    namespace detail
    {
        constexpr size_t WORDS_PER_BITMAP = 64;

        class bitmap
        {
        public:
            [[nodiscard]] size_t find_free_block(size_t size) noexcept;

            void mark_free(size_t index) noexcept;

            [[nodiscard]] bool is_completely_free() const noexcept;

        private:

            std::array<std::atomic<uint64_t>, WORDS_PER_BITMAP> words;
        };
    }

    template<typename T>
    struct allocator_traits
    {
        static constexpr auto object_size = sizeof(T);
        static constexpr auto alignment = alignof(T);
        static constexpr bool is_trivially_destructible = std::is_trivially_destructible_v<T>;
    };

    template
    <
        typename T,
        size_t SlabSize = 4096,
        typename Traits = allocator_traits<T>
    >
    class allocator
    {
    public:
        template<typename... Args>
        [[nodiscard]] T* allocate(Args&&... args) noexcept;

        void deallocate(T* ptr);

        template<size_t N>
       [[nodiscard]] std::array<T*, N> allocate_batch();
    };

    #define DEFINE_ALLOCATOR(Type, Slab) \
    using Type##Allocator = allocator<Type, Slab>
}

#include "allocator.inl"
#include "smart_ptr.inl"