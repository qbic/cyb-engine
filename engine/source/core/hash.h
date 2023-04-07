#pragma once
#include <string>
#include <cassert>

namespace cyb::hash
{
	template <class T>
	constexpr void HashCombine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	constexpr size_t StringHash(const char* input)
	{
		// https://stackoverflow.com/questions/2111667/compile-time-string-hashing
		size_t hash = sizeof(size_t) == 8 ? 0xcbf29ce484222325 : 0x811c9dc5;
		const size_t prime = sizeof(size_t) == 8 ? 0x00000100000001b3 : 0x01000193;

		assert(input != nullptr);
		while (*input)
		{
			hash ^= static_cast<size_t>(*input);
			hash *= prime;
			++input;
		}

		return hash;
	}
}