#pragma once
#include "graphics/graphics-device.h"
#include <unordered_map>
#include "core/enum-flags.h"
#include <memory>

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
        kDiscardFiledataAfterLoad,
        kAllowRetainFiledata
    };

    enum class LoadFlags
    {
        kNone               = 0,
        kFlipImageBit       = (1 << 1),     // Flip image vertically on load
        kRetainFiledataBit  = (1 << 2)      // File data will be kept for later reuse.
    };
    CYB_ENABLE_BITMASK_OPERATORS(LoadFlags);

    // Load a resource:
    //  name : Filename of a resource
    //  flags : Specify flags that modify behaviour (optional)
    //  filedata : Pointer to file data, if file was loaded manually (optional)
    //  filesize : Size of file data, if file was loaded manually (optional)
    Resource Load(
        const std::string& name, 
        LoadFlags flags = LoadFlags::kNone,
        const uint8_t* filedata = nullptr, 
        size_t filesize = 0);

    // Note that even if resource manager is cleared, the resource might still
    // be loaded if anything hold a reference to it.
    void Clear();
};
