#pragma once
#include <atomic>
#include <thread>

class SpinLock
{
private:
	std::atomic_flag lck = ATOMIC_FLAG_INIT;

public:
	void lock()
	{
		int spin = 0;
		while (!try_lock())
		{
			if (spin < 10)
			{
				_mm_pause(); // SMT thread swap can occur here
			}
			else
			{
				std::this_thread::yield(); // OS thread swap can occur here. It is important to keep it as fallback, to avoid any chance of lockup by busy wait
			}
			spin++;
		}
	}
	bool try_lock()
	{
		return !lck.test_and_set(std::memory_order_acquire);
	}
	void unlock()
	{
		lck.clear(std::memory_order_release);
	}
};