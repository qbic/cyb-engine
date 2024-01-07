#pragma once
#include <memory>
#include <functional>

namespace cyb::eventsystem
{
    constexpr int Event_ReloadShaders = -1;
    constexpr int Event_ThreadSafePoint = -2;
    constexpr int Event_SetVSync = -3;
    constexpr int Event_SetFullScreen = -4;

    struct Handle
    {
        std::shared_ptr<void> internal_state;
        [[nodiscard]] bool IsValid() const
        {
            return internal_state.get() != nullptr;
        }
    };

    Handle Subscribe(int id, std::function<void(uint64_t)> callback);
    void Subscribe_Once(int id, std::function<void(uint64_t)> callback);
    void FireEvent(int id, uint64_t userdata);
}