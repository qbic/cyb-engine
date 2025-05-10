#pragma once

namespace cyb
{
    class NonCopyable
    {
    protected:
        NonCopyable() = default;

        NonCopyable(const NonCopyable& other) = delete;
        NonCopyable(NonCopyable&& other) = delete;
        NonCopyable& operator=(const NonCopyable& other) = delete;
        NonCopyable& operator=(NonCopyable&& other) = delete;
    };

    class MovableNonCopyable
    {
    protected:
        MovableNonCopyable() = default;
        MovableNonCopyable(MovableNonCopyable&& other) = default;
        MovableNonCopyable& operator=(MovableNonCopyable&& other) = default;

        MovableNonCopyable(const MovableNonCopyable& other) = delete;
        MovableNonCopyable& operator=(const MovableNonCopyable& other) = delete;
    };
}