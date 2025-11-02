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

namespace cyb {

#ifdef _WIN32
namespace detail
{
class CriticalSection
{
public:
    CriticalSection() { InitializeCriticalSection(&lock_); }
    ~CriticalSection() { DeleteCriticalSection(&lock_); }
    void lock() { EnterCriticalSection(&lock_); }
    bool try_lock() { return TryEnterCriticalSection(&lock_) != FALSE; }
    void unlock() { LeaveCriticalSection(&lock_); }

private:
    CRITICAL_SECTION lock_;
};
}
#endif // _WIN32

//! @brief Platform independent mutex, compatible with @ref ScopedMutex
class Mutex
{
public:
#ifdef _WIN32
    using MutexType = detail::CriticalSection;
#else
    using MutexType = std::mutex;
#endif

    void Lock() { lock_.lock(); }
    [[nodiscard]] bool TryLock() { return lock_.try_lock(); }
    void Unlock() { lock_.unlock(); }

private:
    MutexType lock_;
};

//! @brief A spinlock compatible with @ref ScopedMutex
class SpinLock
{
public:
    void Lock()
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

    [[nodiscard]] bool TryLock()
    {
        return !lock_.test_and_set(std::memory_order_acquire);
    }

    void Unlock()
    {
        lock_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

//! @brief Class with destructor that unlocks mutex
template<typename _Mutex>
class ScopedMutex : private NonCopyable
{
public:
    explicit ScopedMutex(_Mutex& mutex) :
        mutex_(mutex)
    {
        mutex_.Lock();
    }

    ~ScopedMutex()
    {
        mutex_.Unlock();
    }

private:
    _Mutex& mutex_;
};

} // namespace cyb