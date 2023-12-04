#pragma once
#include "graphics/graphics-device.h"
#include "core/enum_flags.h"

namespace cyb
{
    struct Resource
    {
        std::shared_ptr<void> internal_state;

        [[nodiscard]] bool IsValid() const { return internal_state.get() != nullptr; }
        [[nodiscard]] const graphics::Texture& GetTexture() const;
    };
}

namespace cyb::resourcemanager
{
    enum class Mode
    {
        DiscardFiledataAfterLoad,
        AllowRetainFiledata
    };

    enum class Flags
    {
        None = 0,
        FlipImageBit = (1 << 1),            // Flip image vertically on load
        RetainFiledataBit = (1 << 2)        // File data will be kept for later reuse.
    };
    CYB_ENABLE_BITMASK_OPERATORS(Flags);

    // Load a resource:
    //  name : Filename of a resource
    //  flags : Specify flags that modify behaviour (optional)
    //  filedata : Pointer to file data, if file was loaded manually (optional)
    //  filesize : Size of file data, if file was loaded manually (optional)
    [[nodiscard]] Resource Load(
        const std::string& name, 
        Flags flags = Flags::None,
        const uint8_t* filedata = nullptr, 
        size_t filesize = 0);

    // Note that even if resource manager is cleared, the resource might still
    // be loaded if anything hold a reference to it.
    void Clear();
};
