#pragma once
#include <string>
#include <vector>
#include <span>

namespace cyb::filesystem
{
    // Get the extension of a filename in lowercase (dot excluded), if 
    // the file doesn't have any extension an empty string is retuned.
    [[nodiscard]] std::string GetExtension(const std::string& filename);

    // Specify extension without dot eg. "jpg" for ".jpg".
    // This function is case insensitive.
    [[nodiscard]] bool HasExtension(const std::string& filename, const std::string& extension);

    // Read an entire file from the filesystem and store its content in a vector.
    [[nodiscard]] bool ReadFile(const std::string& filename, std::vector<uint8_t>& data);

    // Write a file to the filesystem, if the file already exist it will be trunked.
    bool WriteFile(const std::string& filename, std::span<const std::byte> data);

    /**
     * @brief Write a trivially copyable span to a file.
     */
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    bool WriteFile(const std::string& filename, std::span<const T> s)
    {
        return WriteFile(filename, std::as_bytes(s));
    }

    // Replace backslashes (\) with forward backslashes (/)
    [[nodiscard]] std::string FixFilePath(const std::string& path);
}