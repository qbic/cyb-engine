#pragma once
#include <random>

namespace cyb::random
{
    static inline std::random_device rng_device;
    static inline std::mt19937 generator(rng_device());

    inline uint32_t GenerateInteger(uint32_t min, uint32_t max)
    {
        std::uniform_int_distribution<uint32_t>  distr(min, max);
        return distr(generator);
    }

    inline uint32_t GenerateInteger(uint32_t max)
    {
        return GenerateInteger(0, max);
    }
}