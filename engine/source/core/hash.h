#pragma once
#include <string>
#include <cassert>

namespace cyb::hash
{
	template <class T>
	constexpr void Combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	// Compile-time supported string hasher based on cx_hash.
	// https://github.com/manuelgustavo/cx_hash
	[[nodiscard]] constexpr size_t String(const std::string_view input)
	{
		size_t hash = sizeof(size_t) == 8 ? 0xcbf29ce484222325 : 0x811c9dc5;
		const size_t prime = sizeof(size_t) == 8 ? 0x00000100000001b3 : 0x01000193;

		for (const auto& c: input)
		{
			hash ^= static_cast<size_t>(c);
			hash *= prime;
		}

		return hash;
	}
}