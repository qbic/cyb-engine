#pragma once
#include <immintrin.h>
#include <atomic>
#include <thread>

namespace cyb
{
     /**
     * @brief A spinlock compatible with std::scoped_lock.
     */
    class SpinLock
    {
    public:
        void lock() noexcept
        {
            int spin = 0;

            while (!try_lock())
            {
                // try SMT thread swapping for a few spins by pause(), should be enough
                // most use cases, but put a yield() fallback for OS level thread 
                // swapping to avoid any chance of lockup by busy wait
                if (spin++ < 10)
                    _mm_pause();
                else
                    std::this_thread::yield();
            }
        }

        [[nodiscard]] bool try_lock() noexcept
        {
            return !m_flag.test_and_set(std::memory_order_acquire);
        }

        void unlock() noexcept
        {
            m_flag.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_flag{};
    };
} // namespace cyb