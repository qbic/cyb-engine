#pragma once

#include <unordered_map>
#include <memory>
#include "Graphics/GraphicsDevice.h"

namespace cyb
{
    struct Resource
    {
        std::shared_ptr<void> internal_state;
        inline bool IsValid() const { return internal_state.get() != nullptr; }
        
        const std::vector<uint8_t>& GetFileData() const;
        const graphics::Texture& GetTexture() const;
        
        void SetTexture(const graphics::Texture& texture);
        void SetFileData(const std::vector<uint8_t>& data);
        void SetFileData(std::vector<uint8_t>&& data);
    };
}

namespace cyb::resourcemanager
{
    enum class Mode
    {
        DISCARD_FILEDATA_AFTER_LOAD,
        ALLOW_RETAIN_FILEDATA
    };

    enum Flags
    {
        IMPORT_FLIP_IMAGE = (1 << 1),       // Flip image vertically on load
        IMPORT_RETAIN_FILEDATA = (1 << 2)   // File data will be kept for later reuse.
    };

    // Load a resource:
    //  name : Filename of a resource
    //  flags : Specify flags that modify behaviour (optional)
    //  filedata : Pointer to file data, if file was loaded manually (optional)
    //  filesize : Size of file data, if file was loaded manually (optional)
    Resource Load(
        const std::string& name, 
        uint32_t flags = 0,
        const uint8_t* filedata = nullptr, 
        size_t filesize = 0);

    // Note that even if resource manager is cleared, the resource might still
    // be loaded if anything hold a reference to it.
    void Clear();
};
