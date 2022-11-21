#pragma once
#include <memory>
#include <functional>

namespace cyb::eventsystem
{
    static const int EVENT_RELOAD_SHADERS = -1;
    static const int EVENT_THREAD_SAFE_POINT = -2;
    static const int EVENT_SET_VSYNC = -3;

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