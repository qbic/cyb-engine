#pragma once
#include <atomic>

namespace cyb
{
    template<typename T, size_t Capacity>
    class AtomicQueue
    {
    public:
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2!");

        struct Slot
        {
            std::atomic<size_t> sequence;
            T data;
        };

        Slot buffer[Capacity];
        std::atomic<size_t> enqueuePos{ 0 };
        std::atomic<size_t> dequeuePos{ 0 };

        AtomicQueue()
        {
            for (size_t i = 0; i < Capacity; ++i)
                buffer[i].sequence.store(i, std::memory_order_relaxed);
        }

        bool Push(const T& value)
        {
            size_t pos = enqueuePos.load(std::memory_order_relaxed);
            for (;;)
            {
                Slot& slot = buffer[pos & (Capacity - 1)];
                size_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        slot.data = value;
                        slot.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    // Queue full
                    return false;
                }
                else
                {
                    pos = enqueuePos.load(std::memory_order_relaxed);
                }
            }
        }

        bool Pop(T& result)
        {
            size_t pos = dequeuePos.load(std::memory_order_relaxed);
            for (;;)
            {
                Slot& slot = buffer[pos & (Capacity - 1)];
                size_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        result = slot.data;
                        slot.sequence.store(pos + Capacity, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    // Queue empty
                    return false;
                }
                else
                {
                    pos = dequeuePos.load(std::memory_order_relaxed);
                }
            }
        }
    };
}