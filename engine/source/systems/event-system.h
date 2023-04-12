#pragma once
#include <memory>
#include <functional>

namespace cyb::eventsystem
{
    enum { Event_ReloadShaders = -1 };
    enum { Event_ThreadSafePoint = -2 };
    enum { Event_SetVSync = -3 };

    struct Handle
    {
        std::shared_ptr<void> internal_state;
        inline bool IsValid() const {
            return internal_state.get() != nullptr;
        }
    };

    Handle Subscribe(int id, std::function<void(uint64_t)> callback);
    void Subscribe_Once(int id, std::function<void(uint64_t)> callback);
    void FireEvent(int id, uint64_t userdata);
}