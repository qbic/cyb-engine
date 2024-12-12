#pragma once
#include <random>

namespace cyb::random
{
    inline std::random_device randDevice;
    inline std::mt19937 generator(randDevice());

    // Generate a random integer value between given values.
    template <typename T>
    [[nodiscard]] constexpr T GetNumber(const T min, const T max)
    {
        std::uniform_int_distribution<T> distr(min, max);
        return distr(generator);
    }
}