#pragma once
#include "graphics/device.h"
#include "core/enum_flags.h"

namespace cyb {

    enum class ResourceType {
        None,
        Image,
        Shader,
        Sound
    };

    enum class AssetFlags {
        None = 0,
        RetainFiledataBit = BIT(0),     // file data will be kept for later reuse.
        ImageFipBit = BIT(1)            // flip image vertically on load
    };
    CYB_ENABLE_BITMASK_OPERATORS(AssetFlags);

    using AssetHash = uint64_t;

    class Resource {
    public:
        struct InternalBaseData {
            std::string name;
            AssetHash hash;
            ResourceType type;
            AssetFlags flags;
        };

        Resource() = default;
        Resource(std::shared_ptr<InternalBaseData> data) : m_internalState(data) {}
        virtual ~Resource() = default;

        [[nodiscard]] bool IsValid() const { return m_internalState.get() != nullptr; }
        [[nodiscard]] long GetReferenceCount() const { return IsValid() ? m_internalState.use_count() : 0; }
        [[nodiscard]] ResourceType GetType() const { return IsValid() ? m_internalState->type : ResourceType::None; }
        [[nodiscard]] const graphics::Texture& GetTexture() const;

    private:
        std::shared_ptr<InternalBaseData> m_internalState;
    };
}

namespace cyb::resourcemanager {

    void AddSearchPath(const std::string& path);
    [[nodiscard]] std::string FindFile(const std::string& filename);

    [[nodiscard]] const char* GetTypeAsString(ResourceType type);

    // Load a resource file:
    //  name : Filename of a resource
    //  flags : Specify flags that modify behaviour (optional)
    [[nodiscard]] Resource LoadFile(const std::string& name, AssetFlags flags = AssetFlags::None);

    // Note that even if resource manager is cleared, the resource might still
    // be loaded if anything hold a reference to it.
    void Clear();
};
