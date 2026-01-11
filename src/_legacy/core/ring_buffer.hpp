#pragma once
#include <Arduino.h>
#include <atomic>

template <typename T, size_t N>
class RingBufferSPSC
{
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static constexpr uint32_t MASK = N - 1;
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};
    T buffer_[N];

public:
    bool push(const T &v)
    {
        uint32_t h = head_.load(std::memory_order_relaxed);
        uint32_t n = (h + 1) & MASK;

        if (n == tail_.load(std::memory_order_acquire))
            return false; // full

        buffer_[h] = v;
        head_.store(n, std::memory_order_release);
        return true;
    }

    bool pop(T &v)
    {
        uint32_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return false; // empty
        v = buffer_[t];
        tail_.store((t + 1) & MASK, std::memory_order_release);
        return true;
    }

    uint32_t depth() const
    {
        uint32_t h = head_.load(std::memory_order_acquire);
        uint32_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & MASK;
    }
};