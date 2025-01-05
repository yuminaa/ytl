#pragma once

namespace ytl::detail
{
    inline size_t bitmap::find_free_block(size_t size) noexcept
    {
        for (size_t word_idx = 0; word_idx < WORDS_PER_BITMAP; ++word_idx)
        {
            uint64_t current = words[word_idx].load(std::memory_order_relaxed);
            while(current != 0)
            {
                const size_t bit_pos = __builtin_ctzll(current);
                const size_t block_idx = word_idx * 64 + bit_pos;
                uint64_t expected = current;
                if (const uint64_t desired = current & ~(1ull << bit_pos);
                    words[word_idx].compare_exchange_weak(
                    expected,
                    desired,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)
                    )
                {
                    return block_idx;
                }
                current = words[word_idx].load(std::memory_order_relaxed);
            }
        }
        return static_cast<size_t>(-1);
    }

    inline void bitmap::mark_free(const size_t index) noexcept
    {
        const size_t word_idx = index / 64;
        const size_t bit_pos = index % 64;

        words[word_idx].fetch_or(1ULL << bit_pos, std::memory_order_release);
    }

    inline bool bitmap::is_completely_free() const noexcept
    {
        for (const auto& word : words)
        {
            if (word.load(std::memory_order_relaxed) != ~0ULL)
                return false;
        }
        return true;
    }
}

namespace ytl
{
    template<typename T, size_t SlabSize, typename Traits>
    template<typename... Args>
    T* allocator<T, SlabSize, Traits>::allocate(Args&&... args) noexcept
    {
        void* memory = ::operator new(sizeof(T));

        if constexpr (sizeof...(args) > 0)
            return new(memory) T(std::forward<Args>(args)...);

        return static_cast<T*>(memory);
    }

    template<typename T, size_t SlabSize, typename Traits>
    void allocator<T, SlabSize, Traits>::deallocate(T* ptr)
    {
        if (!ptr)
            return;

        if constexpr (!Traits::is_trivially_destructible)
            ptr->~T();

        ::operator delete(ptr);
    }

    template<typename T, size_t SlabSize, typename Traits>
    template<size_t N>
    std::array<T*, N> allocator<T, SlabSize, Traits>::allocate_batch()
    {
        std::array<T*, N> result;

        for (auto& elem : result)
            elem = allocate();

        return result;
    }
}