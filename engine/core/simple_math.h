#pragma once
#include <cassert>
#include <cmath>
#include <numbers>

namespace cyb
{
    /** Convert angle from degrees to radians. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T ToRadians(const T degrees) noexcept
    {
        return (degrees * std::numbers::pi_v<T>) / T(180);
    }

    /** Convert angle from radians to degrees. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T ToDegrees(const T radians) noexcept
    {
        return (radians * T(180)) / std::numbers::pi_v<T>;
    }

    /** Clamp num to min 0, max 1. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T Clamp01(T x) noexcept
    {
        return std::clamp(x, T(0), T(1));
    }

    /** Check if two floating-point values are approximately equal. */
    template <std::floating_point T>
    [[nodiscard]] constexpr T ApproxEqual(T a, T b, T epsilon = std::numeric_limits<T>::epsilon()) noexcept
    {
        return std::abs(a - b) <= epsilon;
    }

    /** Round up to the next power of two. */
    template <std::integral T>
    [[nodiscard]] constexpr T NextPowerOfTwo(T x) noexcept
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        if constexpr (sizeof(x) == 8)
            x |= x >> 32u;
        return ++x;
    }

    /** Check if a value is a power of two. */
    [[nodiscard]] constexpr bool IsPow2(size_t value) noexcept
    {
        return (value != 0) && ((value & (value - 1)) == 0);
    }

    /** Round up to the next multiple of divisor. Divisor must be a power of two. */
    [[nodiscard]] constexpr uint32_t NextDivisible(uint32_t num, uint32_t divisor) noexcept
    {
        assert(IsPow2(divisor));
        int bits = num & (divisor - 1);
        if (bits == 0)
            return num;

        return num + (divisor - bits);
    }

    /** Align value to the next power of two. */
    [[nodiscard]] constexpr size_t AlignPow2(size_t value, size_t align) noexcept
    {
        assert(IsPow2(align));
        return (value + align - 1) & ~(align - 1);
    }

    /** Align pointer to the next power of two. */
    [[nodiscard]] constexpr void* AlignPow2(void* const ptr, size_t align) noexcept
    {
        assert(IsPow2(align));
        return (void*)(((uintptr_t)ptr + align - 1) & ~(align - 1));
    }
} // namespace cyb