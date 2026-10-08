#pragma once
#include <atomic>
#include <new>
#include <array>

namespace cyb
{
	// A bounded, lock-free, multi-producer, multi-consumer circular m_queue.
	// The capacity must be a power of two, and at least 2.
	template<typename T, size_t N>
	class MPMCQueue
	{
	public:
		static_assert(N >= 2, "Capacity must be at least 2");
		static_assert((N & (N - 1)) == 0, "Capacity must be a power of two");

		struct Slot
		{
			alignas(std::hardware_destructive_interference_size) std::atomic<size_t> sequence;
			T data;
		};

		MPMCQueue()
		{
			for (size_t i = 0; i < m_buffer.size(); ++i)
				m_buffer[i].sequence.store(i, std::memory_order_relaxed);
		}

		// Push a value at the back of the m_queue.
		// Return true if value was successfully enqueued, false if m_queue is full.
		[[nodiscard]] bool Push(T& value) noexcept
		{
			size_t pos = m_enq.load(std::memory_order_relaxed);

			for (;;)
			{
				Slot& s = m_buffer[pos & (N - 1)];
				const size_t seq = s.sequence.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

				if (diff == 0) [[likely]]
				{
					if (m_enq.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						s.data = std::move(value);
						s.sequence.store(pos + 1, std::memory_order_release);
						return true;
					}
				}
				else if (diff < 0) [[unlikely]]
					return false;
				else [[unlikely]]
					m_enq.compare_exchange_weak(pos, seq, std::memory_order_relaxed, std::memory_order_relaxed);
			}
		}
		
		// Pop a value from the front of the m_queue.
		// Returns true if a value was successfully dequeued.
		[[nodiscard]] bool Pop(T& v) noexcept
		{
			size_t pos = m_deq.load(std::memory_order_relaxed);

			for (;;)
			{
				Slot& s = m_buffer[pos & (N - 1)];
				const size_t seq = s.sequence.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

				if (diff == 0) [[likely]]
				{
					if (m_deq.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						v = std::move(s.data);
						s.sequence.store(pos + N, std::memory_order_release);
						return true;
					}
				}
				else if (diff < 0) [[unlikely]]
					return false;
				else [[unlikely]]
					m_deq.compare_exchange_weak(pos, seq - 1, std::memory_order_relaxed, std::memory_order_relaxed);
			}
		}

	private:
		std::array<Slot, N> m_buffer;
		alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_enq{ 0 };
		alignas(std::hardware_destructive_interference_size) std::atomic<size_t> m_deq{ 0 };
	};
} // namespace cyb