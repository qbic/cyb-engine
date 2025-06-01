#pragma once
#include <atomic>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <mutex>
#endif
#include "core/non_copyable.h"

namespace cyb
{
#ifdef _WIN32
    namespace detail
    {
        struct CriticalSection
        {
            CriticalSection() noexcept { InitializeCriticalSection(&m_lock); }
            ~CriticalSection() { DeleteCriticalSection(&m_lock); }

            void lock() { EnterCriticalSection(&m_lock); }
            bool try_lock() { return TryEnterCriticalSection(&m_lock) != FALSE; }
            void unlock() { LeaveCriticalSection(&m_lock); }

        private:
            CRITICAL_SECTION m_lock;
        };
    }
    using MutexType = detail::CriticalSection;
#else
    using MutexType = std::mutex;
#endif

    class SpinLockMutex
    {
    public:
        void Lock() noexcept
        {
            int spin = 0;

            while (!TryLock())
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

        [[nodiscard]] bool TryLock() noexcept
        {
            return !m_lock.test_and_set(std::memory_order_acquire);
        }

        void Unlock() noexcept
        {
            m_lock.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
    };

    class Mutex
    {
    public:
        void Lock() { m_lock.lock(); }
        [[nodiscard]] bool TryLock() { return m_lock.try_lock(); }
        void Unlock() { m_lock.unlock(); }

    private:
        MutexType m_lock;
    };

    template<typename T>
    class ScopedLock : private NonCopyable
    {
    public:
        explicit ScopedLock(T& mutex) : m_mutex(mutex) { m_mutex.Lock(); }
        ~ScopedLock() { m_mutex.Unlock(); }

    private:
        T& m_mutex;
    };
}