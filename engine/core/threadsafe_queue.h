#pragma once
#include <atomic>
#include <optional>

namespace cyb
{
    /**
     * @brief A lockfree, threadsafe, fixedsize, circular queue.
     */
    template<typename T, size_t Capacity>
    class alignas(64) ThreadSafeCircularQueue
    {
    public:
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2!");

        struct Slot
        {
            std::atomic<size_t> sequence;
            T data;
        };

        ThreadSafeCircularQueue()
        {
            for (size_t i = 0; i < Capacity; ++i)
                m_buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
        
        /**
         * @brief Enqueue a value from the back of the queue.
         * @return True if value was successfully enqueued, false is queue is full.
         */
        bool PushBack(T&& value)
        {
            size_t pos = m_enqueuePos.load(std::memory_order_relaxed);
            for (;;)
            {
                Slot& slot = m_buffer[pos & (Capacity - 1)];
                size_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        slot.data = std::move(value);
                        slot.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0)
                {
                    // queue full
                    return false;
                }
                else
                {
                    pos = m_enqueuePos.load(std::memory_order_relaxed);
                }
            }
        }

        /**
         * @brief Dequeue a value from the front of the queue.
         * @return The dequeued value, std::nullopt if queue is empty.
         */
        std::optional<T> PopFront()
        {
            size_t pos = m_dequeuePos.load(std::memory_order_relaxed);
            for (;;)
            {
                Slot& slot = m_buffer[pos & (Capacity - 1)];
                size_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        std::optional<T> result = std::move(slot.data);
                        slot.sequence.store(pos + Capacity, std::memory_order_release);
                        return result;
                    }
                }
                else if (diff < 0)
                {
                    // queue empty
                    return std::nullopt;
                }
                else
                {
                    pos = m_dequeuePos.load(std::memory_order_relaxed);
                }
            }
        }

    private:
        Slot m_buffer[Capacity];
        std::atomic<size_t> m_enqueuePos{ 0 };
        std::atomic<size_t> m_dequeuePos{ 0 };
    };
}