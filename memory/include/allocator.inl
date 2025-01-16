#pragma once

namespace ytl
{
    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    size_t allocator<ps, cls, mc, cs, sc, tc, mp>::find_free_bits(bitmap &bmap, size_t size) noexcept
    {
        const size_t alignment = get_alignment_for_size(size);
        const size_t align_mask = alignment / bitmap::BITS_PER_WORD - 1;

        // PREFETCH_L1(&bmap.words[0]);

        for (size_t i = 0; i < bitmap::WORDS; i += 2)
        {
            if ((i & align_mask) != 0)
                continue;

            const uint64x2_t v = vld1q_u64(reinterpret_cast<const uint64_t*>(&bmap.words[i]));
            const uint64x2_t zero = vdupq_n_u64(0);

            if (const uint64x2_t mask = vceqq_u64(v, zero);
                vgetq_lane_u64(mask, 0) != -1ULL || vgetq_lane_u64(mask, 1) != -1ULL)
            {
                uint64_t word = bmap.words[i].load(std::memory_order_relaxed);
                if (word != 0)
                {
                    const size_t bit = __builtin_ctzll(word);
                    const size_t block_offset = i * 64 + bit;
                    if (bmap.words[i].compare_exchange_weak(
                        word, word & ~(1ULL << bit),
                        std::memory_order_acquire,
                        std::memory_order_relaxed))
                    {
                        // MEMORY_FENCE();
                        return block_offset;
                    }
                }

                // Check second word if first failed
                word = bmap.words[i + 1].load(std::memory_order_relaxed);
                if (word != 0)
                {
                    const size_t bit = __builtin_ctzll(word);
                    const size_t block_offset = (i + 1) * 64 + bit;
                    if (bmap.words[i + 1].compare_exchange_weak(
                        word, word & ~(1ULL << bit),
                        std::memory_order_acquire,
                        std::memory_order_relaxed))
                    {
                        // MEMORY_FENCE();
                        return block_offset;
                    }
                }
            }
        }

        return ~static_cast<size_t>(0);
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::mark_bits_used(bitmap &bmap, size_t idx, size_t size) noexcept
    {
        const size_t word_idx = idx / bitmap::BITS_PER_WORD;
        const size_t bit_idx = idx % bitmap::BITS_PER_WORD;

        // PREFETCH_L1(&bmap.words[word_idx]);
        const uint64_t mask = ~(1ULL << bit_idx);
        if (size >= 128)
        {
            uint64x2_t vmask = vdupq_n_u64(mask);
            uint64x2_t words = vld1q_u64(reinterpret_cast<uint64_t*>(&bmap.words[word_idx]));
            uint64x2_t result = vandq_u64(words, vmask);
            vst1q_u64(reinterpret_cast<uint64_t*>(&bmap.words[word_idx]), result);
            // MEMORY_FENCE();
        } else {
            bmap.words[word_idx].fetch_and(mask, std::memory_order_release);
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::mark_bits_free(bitmap &bmap, size_t idx, size_t size) noexcept
    {
        const size_t word_idx = idx / bitmap::BITS_PER_WORD;
        const size_t bit_idx = idx % bitmap::BITS_PER_WORD;

        // PREFETCH_L1(&bmap.words[word_idx]);

        const uint64_t bits = 1ULL << bit_idx;
        if (size >= 128)
        {
            const uint64x2_t vbits = vdupq_n_u64(bits);
            const uint64x2_t words = vld1q_u64(reinterpret_cast<uint64_t*>(&bmap.words[word_idx]));
            const uint64x2_t result = vorrq_u64(words, vbits);
            vst1q_u64(reinterpret_cast<uint64_t*>(&bmap.words[word_idx]), result);
           // MEMORY_FENCE();
        }
        else
        {
            bmap.words[word_idx].fetch_or(bits, std::memory_order_release);
        }
    }

    // TODO: Add support for other major architectures
    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    bool allocator<ps, cls, mc, cs, sc, tc, mp>::is_bitmap_empty(const bitmap &bmap) noexcept
    {
        uint64x2_t acc = vdupq_n_u64(0xFFFFFFFFFFFFFFFFULL);

        for (size_t i = 0; i < bitmap::WORDS; i += 2)
        {
            uint64x2_t words = vld1q_u64(reinterpret_cast<const uint64_t*>(&bmap.words[i]));
            acc = vandq_u64(acc, words);
        }

        uint64_t result[2];
        vst1q_u64(result, acc);
        return result[0] == 0xFFFFFFFFFFFFFFFFULL && result[1] == 0xFFFFFFFFFFFFFFFFULL;
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void *allocator<ps, cls, mc, cs, sc, tc, mp>::pool_allocate(memory_pool &pool, const size_class &_sc) noexcept
    {
        if (const size_t idx = find_free_bits(pool.bmap, _sc.size);
            idx != ~static_cast<size_t>(0))
        {
            void *block = pool.mem + idx * _sc.slot_size;
            auto *header = new(block) block_header();
            header->data = (_sc.size & SIZE_MASK) | (static_cast<uint64_t>(_sc.size_class) << 48);
            header->magic = HEADER_MAGIC;
            return static_cast<char *>(block) + sizeof(block_header);
        }
        return nullptr;
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::pool_deallocate(memory_pool &pool, void *ptr,
                                                                 const size_class &_sc) noexcept
    {
        const size_t offset = static_cast<const char *>(ptr) - reinterpret_cast<const char *>(pool.mem);
        if (const size_t idx = offset / _sc.slot_size;
            idx < (ps - sizeof(bitmap)) / _sc.slot_size)
        {
            mark_bits_free(pool.bmap, idx);
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    bool allocator<ps, cls, mc, cs, sc, tc, mp>::is_pool_empty(const memory_pool &pool) noexcept
    {
        return is_bitmap_empty();
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void *allocator<ps, cls, mc, cs, sc, tc, mp>::alloc_tiny(const size_t size) noexcept
    {
        const uint8_t size_class = (size - 1) >> 3;
        if (size_class >= tc)
            return nullptr;

        auto &cache = thread_cache.cached_tiny[size_class];
        auto &count = thread_cache.cached_tiny_count[size_class];

        if (count > 0)
            return cache[--count];

        if (!thread_cache.tiny_pools[size_class])
            thread_cache.tiny_pools[size_class] = new tiny_pool();

        const size_t slot_size = size + sizeof(block_header) + cls - 1 & ~(cls - 1);
        tiny_pool &pool = *thread_cache.tiny_pools[size_class];

        if (size_t idx = find_free_bits(pool.bmap, size);
            idx != ~static_cast<size_t>(0))
        {
            void *block = pool.mem + idx * slot_size;
            auto *header = new(block) block_header();
            header->data = (size & SIZE_MASK) | (static_cast<uint64_t>(size_class) << 48);
            header->magic = HEADER_MAGIC;
            return static_cast<char *>(block) + sizeof(block_header);
        }

        return nullptr;
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void *allocator<ps, cls, mc, cs, sc, tc, mp>::alloc_small(size_t size) noexcept
    {
        if (!thread_cache.pool_mgr)
            thread_cache.pool_mgr = new pool_manager();

        const size_t size_class = 31 - __builtin_clz(size - 1);
        const auto &sc1 = size_class_table[size_class];

        auto &pools = thread_cache.pool_mgr->pools[size_class];
        auto &count = thread_cache.pool_mgr->counts[size_class];

        for (size_t i = 0; i < count; ++i)
        {
            if (void *ptr = pool_allocate(*pools[i].pool, sc1))
            {
                ++pools[i].used;
                return ptr;
            }
        }

        if (count >= mp)
            return nullptr;

        try
        {
            auto *new_pool = new(std::align_val_t { ps }) memory_pool();
            pools[count] = { new_pool, 1 };
            ++count;
            return pool_allocate(*new_pool, sc1);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void *allocator<ps, cls, mc, cs, sc, tc, mp>::alloc_large(const size_t size) noexcept
    {
        const size_t total_size = size + sizeof(block_header);
        const size_t alloc_size = (total_size + ps - 1) & ~(ps - 1);

        void *ptr = MAP_MEMORY(alloc_size);
        if (ptr == MAP_FAILED)
            return nullptr;

        auto *header = new(ptr) block_header();
        header->data = (size & SIZE_MASK) | (static_cast<uint64_t>(255) << 48) | MMAP_FLAG;
        header->magic = HEADER_MAGIC;
        return static_cast<char *>(ptr) + sizeof(block_header);
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::free_tiny(void *ptr) noexcept
    {
        auto *header = reinterpret_cast<block_header *>(static_cast<char *>(ptr) - sizeof(block_header));

        const uint8_t size_class = (header->data & CLASS_MASK) >> 48;
        if (size_class >= tc)
            return;

        auto &count = thread_cache.cached_tiny_count[size_class];
        if (count < cs)
        {
            thread_cache.cached_tiny[size_class][count++] = ptr;
            return;
        }

        if (auto *pool = thread_cache.tiny_pools[size_class])
        {
            const size_t offset = static_cast<char *>(ptr) - sizeof(block_header) - pool->mem;
            const size_t size = (size_class + 1) << 3;
            const size_t slot_size = size + sizeof(block_header) + cls - 1 & ~(cls - 1);
            const size_t idx = offset / slot_size;
            mark_bits_free(pool->bmap, idx);
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::free_small(void *ptr) noexcept
    {
        auto *header = reinterpret_cast<block_header *>(static_cast<char *>(ptr) - sizeof(block_header));

        const uint8_t size_class = (header->data & CLASS_MASK) >> 48;
        if (!thread_cache.pool_mgr)
            return;

        auto &pools = thread_cache.pool_mgr->pools[size_class];
        auto &count = thread_cache.pool_mgr->counts[size_class];

        for (size_t i = 0; i < count; ++i)
        {
            auto &entry = pools[i];
            if (reinterpret_cast<uintptr_t>(ptr) - sizeof(block_header) >= reinterpret_cast<uintptr_t>(entry.pool) &&
                reinterpret_cast<uintptr_t>(ptr) - sizeof(block_header) < reinterpret_cast<uintptr_t>(entry.pool) + ps)
            {
                pool_deallocate(*entry.pool, ptr, size_class_table[size_class]);
                if (--entry.used == 0 && is_pool_empty(*entry.pool))
                {
                    delete entry.pool;
                    entry = pools[--count];
                }
                return;
            }
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::free_large(void *ptr) noexcept
    {
        auto *header = reinterpret_cast<block_header *>(static_cast<char *>(ptr) - sizeof(block_header));
        if (!(header->data & MMAP_FLAG))
            return;

        const size_t size = header->data & SIZE_MASK;
        const size_t total_size = size + sizeof(block_header);
        const size_t alloc_size = (total_size + ps - 1) & ~(ps - 1);
        UNMAP_MEMORY(static_cast<char*>(ptr) - sizeof(block_header), alloc_size);
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void *allocator<ps, cls, mc, cs, sc, tc, mp>::allocate(size_t size) noexcept
    {
        if (size == 0 || size > 1ULL << 47)
            return nullptr;

        if (size <= TINY_THRESHOLD)
            return alloc_tiny(size);

        if (size <= SMALL_THRESHOLD)
            return alloc_small(size);

        if (size < LARGE_THRESHOLD)
        {
            return alloc_small(size);
        }

        return alloc_large(size);
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::deallocate(void *ptr) noexcept
    {
        if (!ptr)
            return;

        auto *header = reinterpret_cast<block_header *>(static_cast<char *>(ptr) - sizeof(block_header));

        if (const uint8_t size_class = (header->data & CLASS_MASK) >> 48;
            size_class < tc)
        {
            free_tiny(ptr);
        }
        else if (!(header->data & MMAP_FLAG))
        {
            free_small(ptr);
        }
        else
        {
            free_large(ptr);
        }
    }

    template<size_t ps, size_t cls, size_t mc, size_t cs, size_t sc, size_t tc, size_t mp>
    void allocator<ps, cls, mc, cs, sc, tc, mp>::cleanup() noexcept
    {
        for (auto &pool: thread_cache.tiny_pools)
        {
            if (pool)
            {
                delete pool;
                pool = nullptr;
            }
        }

        if (thread_cache.pool_mgr)
        {
            for (size_t sc = 0; sc < size_classes; ++sc)
            {
                auto &count = thread_cache.pool_mgr->counts[sc];
                for (size_t i = 0; i < count; ++i)
                {
                    delete thread_cache.pool_mgr->pools[sc][i].pool;
                }
                count = 0;
            }
            delete thread_cache.pool_mgr;
            thread_cache.pool_mgr = nullptr;
        }

        for (size_t i = 0; i < tiny_classes; ++i)
            thread_cache.cached_tiny_count[i] = 0;

        for (size_t i = 0; i < size_classes; ++i)
            thread_cache.cached_small_count[i] = 0;
    }
}
