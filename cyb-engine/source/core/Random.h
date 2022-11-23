#pragma once
#include <random>

namespace cyb::random
{
    static inline std::random_device randDevice;
    static inline std::mt19937 generator(randDevice());

    inline uint32_t GenerateInteger(uint32_t min, uint32_t max)
    {
        std::uniform_int_distribution<uint32_t> distr(min, max);
        return distr(generator);
    }
}