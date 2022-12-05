#pragma once
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <cassert>

namespace cyb::helper
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

		while (*input)
		{
			hash ^= static_cast<size_t>(*input);
			hash *= prime;
			++input;
		}

		return hash;
	}

	std::string ToUpper(const std::string& s);
	std::string GetExtensionFromFileName(const std::string& filename);
	std::string GetBasePath(const std::string& path);

	bool FileRead(const std::string& filename, std::vector<uint8_t>& data, bool quiet = false);
	bool FileWrite(const std::string& filename, const uint8_t* data, size_t size);
	bool FileExist(const std::string& filename);


	void TokenizeString(std::string const& str, const char delim, std::vector<std::string>& out);
	std::string JoinStrings(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, const char delim);

	enum class FileOp
	{
		OPEN,
		SAVE
	};

	void FileDialog(FileOp mode, const std::string& filters, std::function<void(std::string fileName)> onSuccess);

	void ShaderDebugDialog(const std::string& source, const std::string& errorMessage);

	inline uint32_t SafeTruncateToU32(const uint64_t a)
	{
		assert(a <= 0xffffffff);
		const uint32_t result = (uint32_t)a;
		return result;
	}

	inline uint16_t SafeTruncateToU16(const uint64_t a)
	{
		assert(a <= 0xffff);
		const uint16_t result = (uint16_t)a;
		return result;
	}

	template <typename T, typename U>
	void SetArray(T* array, U x) 
	{
		*array = x;
	}

	template <typename T, typename U, typename... V>
	void SetArray(T* array, U x, V... y) {
		*array = x;
		SetArray(array + 1, y...);
	}
};