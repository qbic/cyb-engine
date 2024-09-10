#pragma once
#include <atomic>
#include <thread>

class SpinLock {
public:
	void lock() noexcept {
		int spin = 0;

		while (!try_lock()) {
			if (spin < 10) {
				// SMT thread swap can occur here
				_mm_pause();
			} else {
				// OS thread swap can occur here. It is important to keep it as fallback, 
				// to avoid any chance of lockup by busy wait
				std::this_thread::yield();
			}
			spin++;
		}
	}

	[[nodiscard]] inline bool try_lock() noexcept {
		return !m_lock.test_and_set(std::memory_order_acquire);
	}

	void unlock() noexcept {
		m_lock.clear(std::memory_order_release);
	}

private:
	std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};