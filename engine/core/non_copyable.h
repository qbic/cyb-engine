#pragma once

namespace cyb
{
    class NonCopyable
    {
    protected:
        NonCopyable() = default;
        NonCopyable(NonCopyable&& other) = default;
        NonCopyable& operator=(NonCopyable&& other) = default;
        NonCopyable(const NonCopyable& other) = delete;
        NonCopyable& operator=(const NonCopyable& other) = delete;
    };

    class NonCopyableNonMovable
    {
    protected:
        NonCopyableNonMovable() = default;
        NonCopyableNonMovable(const NonCopyableNonMovable& other) = delete;
        NonCopyableNonMovable(NonCopyableNonMovable&& other) = delete;
        NonCopyableNonMovable& operator=(const NonCopyableNonMovable& other) = delete;
        NonCopyableNonMovable& operator=(NonCopyableNonMovable&& other) = delete;
    };

} // namespace cyb