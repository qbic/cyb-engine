#pragma once
#include <string_view>

namespace cyb
{
    template <class T>
    constexpr void HashCombine(uint64_t& seed, const T& v) noexcept
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }

    // Compile-time supported string hasher based on cx_hash.
    // https://github.com/manuelgustavo/cx_hash
    [[nodiscard]] constexpr uint64_t HashString(std::string_view input) noexcept
    {
        uint64_t hash = 0xcbf29ce484222325;
        const uint64_t prime = 0x00000100000001b3;

        for (const auto& c : input)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= prime;
        }

        return hash;
    }
} // namespace cyb
