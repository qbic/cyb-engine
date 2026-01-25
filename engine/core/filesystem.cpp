#include <algorithm>
#include <cassert>
#include <fstream>
#include <thread>
#include "core/logger.h"
#include "core/filesystem.h"

namespace cyb::filesystem
{
    std::string GetExtension(const std::string& filename)
    {
        const size_t index = filename.rfind('.');
        if (index == std::string::npos)
            return std::string();   // no extension was found

        std::string ext = filename.substr(index + 1);
        std::transform(ext.cbegin(), ext.cend(), ext.begin(), ::tolower);
        return ext;
    }

    bool HasExtension(const std::string& filename, const std::string& extension)
    {
        const std::string fileExtension = GetExtension(filename);
        if (fileExtension.empty() || fileExtension.size() > extension.size())
            return false;           // size mismatch

        for (size_t i = 0; i < fileExtension.size(); ++i)
            if (std::tolower(fileExtension[i] != extension[i]))
                return false;       // char mismatch

        return true;
    }

    bool ReadFile(const std::string& filename, std::vector<uint8_t>& data)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            CYB_ERROR("Failed to open file (filename={0}): {1}", filename, strerror(errno));
            return false;
        }

        std::streampos size = file.tellg();
        file.seekg(0, file.beg);
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return true;
    }

    bool WriteFile(const std::string& filename, std::span<const std::byte> data)
    {
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            CYB_ERROR("Failed to write file (filename={0}): {1}\n", filename, strerror(errno));
            return false;
        }

        file.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));

        return file.good();
    }

    std::string FixFilePath(const std::string& path)
    {
        std::string newStr = path;
        for (auto & c : newStr)
        {
            if (c == '\\')
                c = '/';
        }

        return newStr;
    }
}