#pragma once
#include "graphics/device.h"
#include "core/enum_flags.h"

namespace cyb
{
    enum class ResourceType
    {
        None,
        Image,
        Shader,
        Sound
    };

    enum class AssetFlags
    {
        None = 0,
        RetainFiledataBit = BIT(0),     // file data will be kept for later reuse.
        ImageFipBit = BIT(1),           // flip image vertically on load
    };
    CYB_ENABLE_BITMASK_OPERATORS(AssetFlags);

    class Resource
    {
    public:
        struct InternalBaseData
        {
            std::string name;
            uint64_t hash = 0;
            ResourceType type = ResourceType::None;
            AssetFlags flags = AssetFlags::None;
        };

        Resource() = default;
        Resource(std::shared_ptr<InternalBaseData> data) : m_internalState(data) {}
        virtual ~Resource() = default;

        [[nodiscard]] bool IsValid() const { return m_internalState.get() != nullptr; }
        [[nodiscard]] long GetReferenceCount() const { return IsValid() ? m_internalState.use_count() : 0; }
        [[nodiscard]] ResourceType GetType() const { return IsValid() ? m_internalState->type : ResourceType::None; }
        [[nodiscard]] const rhi::Texture& GetTexture() const;

    private:
        std::shared_ptr<InternalBaseData> m_internalState;
    };
}

namespace cyb::resourcemanager
{
    void Initialize();
    void AddSearchPath(const std::string& path);

    /**
     * @brief Try to resolve asset type though parsing the filename.
     * 
     * @param type Pointer to where to store resource type. Can be nullptr.
     * @param filename Filename of the asset to resolve.
     * 
     * @return True if successfully parsed asset type, else false.
     */
    [[nodiscard]] bool GetAssetTypeFromFilename(ResourceType* type, const std::string& filename);

    /**
     * @brief Try to locate a file in any of the added search paths.
     * @return Full path to file if found, else just filename.
     */
    [[nodiscard]] std::string FindFile(const std::string& filename);

    /**
     * @brief Get resource type as string.
     */
    [[nodiscard]] const char* GetTypeAsString(ResourceType type);

    /**
     * @brief Load a resource from file.
     * 
     * Internally searched for file though \see FindFile.
     * Type will automaticly by deduced by extension.
     * 
     * @param name Relative path to file.
     * @param flags (optional) Specify flags that modify behaviour.
     * @param force (optional) Forces loading even if file is allready in cache.
     * @return A reference counted Resource object. May be invalid if loading failed.
     */
    [[nodiscard]] Resource LoadFile(const std::string& name, AssetFlags flags = AssetFlags::None, bool force = false);

    /**
     * @brief Clear resource manager from all loaded assets.
     * 
     * Note that even though resource manager is cleared, resources may
     * still be valid as long as anything holds a refence to it.
     */
    void Clear();
};
