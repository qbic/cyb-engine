#include <unordered_map>
#include <vector>
#include <list>
#include <mutex>
#include "systems/event_system.h"

namespace cyb::eventsystem
{
    struct EventManager
    {
        std::unordered_map<int, std::list<std::function<void(uint64_t)>*>> subscribers;
        std::unordered_map<int, std::vector<std::function<void(uint64_t)>>> subscribers_once;
        std::mutex locker;
    };
    std::shared_ptr<EventManager> manager = std::make_shared<EventManager>();

	struct EventInternal
	{
		std::shared_ptr<EventManager> manager;
		int id = 0;
		Handle handle;
		std::function<void(uint64_t)> callback;

		~EventInternal()
		{
			std::scoped_lock lock(manager->locker);
			auto it = manager->subscribers.find(id);
			if (it != manager->subscribers.end())
				it->second.remove(&callback);
		}
	};

	Handle Subscribe(int id, std::function<void(uint64_t)> callback)
	{
		Handle handle;
		auto eventinternal = std::make_shared<EventInternal>();
		handle.internal_state = eventinternal;
		eventinternal->manager = manager;
		eventinternal->id = id;
		eventinternal->handle = handle;
		eventinternal->callback = callback;

		manager->locker.lock();
		manager->subscribers[id].push_back(&eventinternal->callback);
		manager->locker.unlock();

		return handle;
	}

	void Subscribe_Once(int id, std::function<void(uint64_t)> callback)
	{
		manager->locker.lock();
		manager->subscribers_once[id].push_back(callback);
		manager->locker.unlock();
	}

	void FireEvent(int id, uint64_t userdata)
	{
		// Callbacks that only live for once:
		{
			manager->locker.lock();
			auto it = manager->subscribers_once.find(id);
			bool found = it != manager->subscribers_once.end();

			if (found)
			{
				auto& callbacks = it->second;
				for (auto& callback : callbacks)
					callback(userdata);

				callbacks.clear();
			}
			manager->locker.unlock();
		}
		// Callbacks that live until deleted:
		{
			manager->locker.lock();
			auto it = manager->subscribers.find(id);
			bool found = it != manager->subscribers.end();
			manager->locker.unlock();

			if (found)
			{
				auto& callbacks = it->second;
				for (auto& callback : callbacks)
				{
					(*callback)(userdata);
				}
			}
		}
		
	}
}