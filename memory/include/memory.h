#pragma once

#include <arm_neon.h>
#include <array>
#include <cstddef>
#include <mach/mach.h>
#include <sys/mman.h>

#define MAP_MEMORY(size) mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
#define UNMAP_MEMORY(ptr, size) munmap(ptr, size)
#define ALIGNED_ALLOC(alignment, size) aligned_alloc(alignment, size)
#define ALIGNED_FREE(ptr) free(ptr)

#define CACHE_LINE_SIZE 64
#define PG_SIZE 4096

namespace ytl
{
    void *memset(void *s, int c, size_t count) noexcept;

    void *memcpy(void *dest, const void *src, size_t count) noexcept;

    void *memmove(void *dest, const void *src, size_t count) noexcept;

    int memcmp(const void *cs, const void *ct, size_t count) noexcept;

    template<
        size_t page_size = 4096,
        size_t cache_line_size = 64,
        size_t max_cached = 32,
        size_t cache_size = 32,
        size_t size_classes = 32,
        size_t tiny_classes = 8,
        size_t max_pools = 8
    >
    class allocator
    {
        static constexpr uint64_t SIZE_MASK = 0x0000FFFFFFFFFFFF;
        static constexpr uint64_t CLASS_MASK = 0x00FF000000000000;
        static constexpr uint64_t MMAP_FLAG = 1ULL << 62;
        static constexpr uint64_t HEADER_MAGIC = 0xDEADBEEF12345678;

        static constexpr size_t TINY_THRESHOLD = 64;
        static constexpr size_t SMALL_THRESHOLD = 256;
        static constexpr size_t LARGE_THRESHOLD = 1024 * 1024;

        struct size_class
        {
            uint16_t size;
            uint16_t slot_size;
            uint16_t blocks;
            uint16_t slack;
        };

        static constexpr size_t get_alignment_for_size(const size_t size) noexcept
        {
            return size <= cache_line_size
                       ? cache_line_size
                       : size >= page_size
                             ? page_size
                             : 1ULL << (64 - __builtin_clzll(size - 1));
        }

        static constexpr auto init_size_classes() noexcept
        {
            std::array<size_class, size_classes> classes {};
            for (size_t i = 0; i < size_classes; ++i)
            {
                const size_t size = 1ULL << (i + 3);
                const size_t alignment = get_alignment_for_size(size);
                const size_t slot = (size + alignment - 1) & ~(alignment - 1);
                classes[i] = {
                    static_cast<uint16_t>(size),
                    static_cast<uint16_t>(slot),
                    static_cast<uint16_t>(page_size / slot),
                    static_cast<uint16_t>(slot - size)
                };
            }
            return classes;
        }

        static constexpr std::array<size_class, size_classes> size_class_table = init_size_classes();

        struct alignas(cache_line_size) block_header
        {
            uint64_t data;
            uint64_t magic;
            block_header *prev;
            block_header *next;
        };

        struct alignas(cache_line_size) bitmap
        {
            static constexpr size_t BITS_PER_WORD = 64;
            static constexpr size_t WORDS = 4;
            std::atomic<uint64_t> words[WORDS];
        };

        struct alignas(page_size) memory_pool
        {
            bitmap bmap;
            uint8_t mem[page_size - sizeof(bitmap)] {};
        };

        struct alignas(page_size) tiny_pool
        {
            bitmap bmap;
            alignas(cache_line_size) uint8_t mem[page_size - sizeof(bitmap)] {};
        };

        struct pool_entry
        {
            memory_pool *pool;
            size_t used;
        };

        struct pool_manager
        {
            alignas(cache_line_size) pool_entry pools[size_classes][max_pools];
            size_t counts[size_classes];
        };

        thread_local static struct thread_cache_t
        {
            std::array<tiny_pool *, tiny_classes> tiny_pools;
            pool_manager *pool_mgr;
            void *cached_tiny[tiny_classes][cache_size];
            size_t cached_tiny_count[tiny_classes];
            void *cached_small[size_classes][cache_size];
            size_t cached_small_count[size_classes];
        } thread_cache;

        static size_t find_free_bits(bitmap &bmap, size_t size) noexcept;

        static void mark_bits_used(bitmap &bmap, size_t idx, size_t size) noexcept;

        static void mark_bits_free(bitmap &bmap, size_t idx, size_t size) noexcept;

        static bool is_bitmap_empty(const bitmap &bmap) noexcept;

        static void *pool_allocate(memory_pool &pool, const size_class &_sc) noexcept;

        static void pool_deallocate(memory_pool &pool, void *ptr, const size_class &_sc) noexcept;

        static bool is_pool_empty(const memory_pool &pool) noexcept;

        static void *alloc_tiny(size_t size) noexcept;

        static void *alloc_small(size_t size) noexcept;

        static void *alloc_large(size_t size) noexcept;

        static void free_tiny(void *ptr) noexcept;

        static void free_small(void *ptr) noexcept;

        static void free_large(void *ptr) noexcept;

    public:
        static void *allocate(size_t size) noexcept;

        static void deallocate(void *ptr) noexcept;

        static void cleanup() noexcept;
    };

    template<typename T>
    struct default_delete;

    template<typename T, typename Deleter = default_delete<T> >
    class unique_ptr
    {
        T *ptr_ { nullptr };
        [[no_unique_address]] Deleter deleter_ {};

    public:
        constexpr unique_ptr() noexcept = default;

        ~unique_ptr() noexcept;

        unique_ptr(const unique_ptr &) = delete;

        unique_ptr &operator=(const unique_ptr &) = delete;

        unique_ptr(unique_ptr &&) noexcept = default;

        unique_ptr &operator=(unique_ptr &&) noexcept = default;
    };

    struct control_block
    {
        std::atomic<size_t> shared_count { 1 };
        std::atomic<size_t> weak_count { 0 };

        virtual void destroy() noexcept = 0;

        virtual ~control_block() = default;
    };

    template<typename T>
    class shared_ptr
    {
        T *ptr_ { nullptr };
        control_block *ctrl_ { nullptr };

    public:
        constexpr shared_ptr() noexcept = default;

        ~shared_ptr() noexcept;

        shared_ptr(const shared_ptr &other) noexcept;

        shared_ptr &operator=(const shared_ptr &other) noexcept;

        shared_ptr(shared_ptr &&other) noexcept;

        shared_ptr &operator=(shared_ptr &&other) noexcept;
    };

    template<typename T>
    class weak_ptr
    {
        T *ptr_ { nullptr };
        control_block *ctrl_ { nullptr };

    public:
        constexpr weak_ptr() noexcept = default;

        ~weak_ptr() noexcept;

        weak_ptr(const weak_ptr &other) noexcept;

        weak_ptr &operator=(const weak_ptr &other) noexcept;

        weak_ptr(weak_ptr &&other) noexcept;

        weak_ptr &operator=(weak_ptr &&other) noexcept;

        shared_ptr<T> lock() const noexcept;
    };

    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args &&... args) noexcept;

    template<typename T>
    unique_ptr<T> make_unique_for_overwrite() noexcept;

    template<typename T, typename... Args>
    shared_ptr<T> make_shared(Args &&... args) noexcept;

    template<typename T>
    shared_ptr<T> make_shared_for_overwrite() noexcept;

    template<typename T>
    unique_ptr<T> make_unique_array(size_t size) noexcept;

    template<typename T>
    shared_ptr<T> make_shared_array(size_t size) noexcept;
}

#include "allocator.inl"
#include "smart_ptr.inl"
