#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <new>
#include <cstring>
#include <immintrin.h> // _mm_pause

// Cache line size for x86-64
static constexpr size_t CACHE_LINE_SIZE = 64;

// portable aligned alloc/free
static inline void *aligned_alloc_portable(size_t alignment, size_t size)
{
#ifdef _MSC_VER
    return _aligned_malloc(size, alignment);
#else
    void *ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0)
        return nullptr;
    return ptr;
#endif
}
static inline void aligned_free_portable(void *p)
{
#ifdef _MSC_VER
    _aligned_free(p);
#else
    free(p);
#endif
}

// Bounded MPMC ring buffer (Vyukov style). Works correctly for SPMC (single producer, multiple consumers)
// and also supports multiple producers if needed.
template <typename T>
class alignas(CACHE_LINE_SIZE) AtomicRingBuffer
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for this implementation");

    struct Cell
    {
        std::atomic<size_t> seq;
        alignas(CACHE_LINE_SIZE) T data;
    };

public:
    explicit AtomicRingBuffer(size_t size)
        : capacity_(nextPowerOf2(size)), mask_(capacity_ - 1),
          buffer_(nullptr), head_(0), tail_(0)
    {
        assert((capacity_ & (capacity_ - 1)) == 0);
        buffer_ = static_cast<Cell *>(aligned_alloc_portable(CACHE_LINE_SIZE, sizeof(Cell) * capacity_));
        assert(buffer_ != nullptr);
        for (size_t i = 0; i < capacity_; ++i)
        {
            new (&buffer_[i]) Cell();
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ~AtomicRingBuffer()
    {
        if (buffer_)
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                buffer_[i].~Cell();
            }
            aligned_free_portable(buffer_);
        }
    }

    AtomicRingBuffer(const AtomicRingBuffer &) = delete;
    AtomicRingBuffer &operator=(const AtomicRingBuffer &) = delete;

    // push single item. returns true if success, false if full.
    bool push(const T &item) noexcept
    {
        size_t pos = tail_.load(std::memory_order_relaxed);
        for (;;)
        {
            Cell &cell = buffer_[pos & mask_];
            size_t seq = cell.seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                // try to claim this slot
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    // claimed, write data
                    std::memcpy(&cell.data, &item, sizeof(T));
                    cell.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed -> pos updated with new tail, retry
            }
            else if (dif < 0)
            {
                // queue full
                return false;
            }
            else
            {
                // seq > pos => slot not ready for producer; refresh pos and retry
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    // pop single item. returns true if success, false if empty.
    bool pop(T &item) noexcept
    {
        size_t pos = head_.load(std::memory_order_relaxed);
        for (;;)
        {
            Cell &cell = buffer_[pos & mask_];
            size_t seq = cell.seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                // try to claim this slot
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    std::memcpy(&item, &cell.data, sizeof(T));
                    cell.seq.store(pos + capacity_, std::memory_order_release);
                    return true;
                }
                // CAS failed -> pos updated, retry
            }
            else if (dif < 0)
            {
                // no data
                return false;
            }
            else
            {
                // seq > pos+1 => consumer lost race, refresh pos
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    // pushBatch: try to push up to count items. returns number pushed.
    size_t pushBatch(const T *items, size_t count) noexcept
    {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i)
        {
            if (!push(items[i]))
                break;
            pushed++;
        }
        return pushed;
    }

    // popBatch: try to pop up to max_count items into items[]. returns number popped.
    size_t popBatch(T *items, size_t max_count) noexcept
    {
        size_t popped = 0;
        // try to pop contiguous items until empty or max_count
        for (size_t i = 0; i < max_count; ++i)
        {
            if (!pop(items[i]))
                break;
            popped++;
        }
        return popped;
    }

    bool empty() const noexcept
    {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return h == t;
    }

    bool full() const noexcept
    {
        // When tail - head == capacity => full
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_acquire);
        return (t - h) >= capacity_;
    }

    size_t size() const noexcept
    {
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_acquire);
        return t - h;
    }

    size_t capacity() const noexcept { return capacity_; }
    size_t available() const noexcept { return capacity_ - size(); }

    void clear() noexcept
    {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        for (size_t i = 0; i < capacity_; ++i)
        {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

private:
    static size_t nextPowerOf2(size_t n)
    {
        if (n <= 1)
            return 2;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_t) > 4)
            n |= n >> 32;
        return n + 1;
    }

    const size_t capacity_;
    const size_t mask_;
    Cell *buffer_;

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
};