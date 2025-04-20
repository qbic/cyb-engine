#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>
#include "core/platform.h"

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

    // Write a file to the filesystem, is the file allready exist it will be trunced.
    [[nodiscard]] bool WriteFile(const std::string& filename, const uint8_t* data, size_t size);

    // Open a file browser dialog window for opening a file on a seperate thread.
    // If the user clicks on "Open", onSuccess is executed
    void OpenDialog(const std::string& filters, std::function<void(std::string filename)> onSuccess);
    
    // Open a file browser dialog window for saving a file on a seperate thread.
    // If the user clicks on "Save", onSuccess is executed.
    void SaveDialog(const std::string& filters, std::function<void(std::string filename)> onSuccess);

    // Replace backslashes (\) with forward backslashs (/)
    std::string FixFilePath(const std::string& path);
}