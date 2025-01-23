#pragma once
#include <atomic>
#include <thread>

class SpinLock
{
public:
    void lock() noexcept
    {
        int spin = 0;

        while (!try_lock())
        {
            // try SMT thread swapping for a few spins by pause(), should be enough
            // most use cases, but put a yeald() fallback for OS level thread 
            // swapping to avoid any chance of lockup by busy wait
            if (spin++ < 10)
                _mm_pause();
            else
                std::this_thread::yield();
        }
    }

    [[nodiscard]] inline bool try_lock() noexcept
    {
        return !m_lock.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        m_lock.clear(std::memory_order_release);
    }

private:
    std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};
