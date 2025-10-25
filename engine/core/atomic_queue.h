#pragma once
#include <atomic>
#include <new>
#include <optional>

namespace cyb {

/**
 * @brief An atomic, MPMC circular queue.
 */
template<typename T, size_t capacity>
class AtomicCircularQueue
{
public:
    static_assert(capacity >= 2, "Capacity must be at least 2");

    struct Slot
    {
        std::atomic_size_t sequence;
        T value;
    };

    AtomicCircularQueue()
    {
        for (size_t i = 0; i < capacity; ++i)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    /**
     * @brief Enqueue a value at the back of the queue.
     * @return True if value was successfully enqueued, false is queue is full.
     */
    bool Enqueue(T&& value)
    {
        Slot* slot = nullptr;
        size_t pos = enqueuePos_.load(std::memory_order_relaxed);

        for (;;)
        {
            slot = &buffer_[pos % capacity];
            const size_t seq = slot->sequence.load(std::memory_order_acquire);

            // diff == 0: Slot is free to write
            // diff < 0: Slot still in use (full)
            // diff > 0: Another thread advanced ahead
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff < 0) [[unlikley]]
                return false;

            if (diff == 0) [[likley]]
            {
                if (enqueuePos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acquire, std::memory_order_relaxed))
                    break;
            }

            // CAS failed, reload pos
            pos = enqueuePos_.load(std::memory_order_relaxed);
        }

        slot->value = std::move(value);
        slot->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Dequeue a value from the front of the queue.
     * @return The dequeued value, std::nullopt if queue is empty.
     */
    std::optional<T> Dequeue()
    {
        Slot* slot = nullptr;
        size_t pos = dequeuePos_.load(std::memory_order_relaxed);

        for (;;)
        {
            slot = &buffer_[pos % capacity];
            const size_t seq = slot->sequence.load(std::memory_order_acquire);

            // diff == 0: Slot is ready to consume
            // diff < 0: Empty queue
            // diff > 0: Another consumer advanced (retry)
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff < 0) [[unlikey]]
                return std::nullopt;

            if (diff == 0) [[likley]]
            {
                if (dequeuePos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acquire, std::memory_order_relaxed))
                    break;
            }

            // CAS failed, reload pos
            pos = dequeuePos_.load(std::memory_order_relaxed);
        }

        slot->sequence.store(pos + capacity, std::memory_order_release);
        return std::move(slot->value);
    }

private:
    Slot buffer_[capacity];
    alignas(std::hardware_destructive_interference_size) std::atomic_size_t enqueuePos_{ 0 };
    alignas(std::hardware_destructive_interference_size) std::atomic_size_t dequeuePos_{ 0 };
};

} // namespace cyb