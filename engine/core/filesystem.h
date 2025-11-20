#pragma once
#include <string>
#include <vector>
#include <functional>
#include <span>

namespace cyb::filesystem
{
    // Get the extension of a filename in lowercase (dot excluded), if 
    // the file doesen't have any extension an empty string is retuned.
    [[nodiscard]] std::string GetExtension(const std::string& filename);

    // Specify extension without dot eg. "jpg" for ".jpg".
    // This function is case insesitive.
    [[nodiscard]] bool HasExtension(const std::string& filename, const std::string& extension);

    // Read an entire file from the filesystem and store its content in a vector.
    [[nodiscard]] bool ReadFile(const std::string& filename, std::vector<uint8_t>& data);

    // Write a file to the filesystem, if the file allready exist it will be trunced.
    [[nodiscard]] bool WriteFile(const std::string& filename, std::span<const uint8_t> data);

    // Replace backslashes (\) with forward backslashs (/)
    std::string FixFilePath(const std::string& path);
}