#include <filesystem>
#include <mutex>
#include "core/cvar.h"
#include "core/dir-watcher.h"
#include "core/filesystem.h"
#include "core/hash.h"
#include "core/logger.h"
#include "core/timer.h"
#include "graphics/device.h"
#include "systems/resource_manager.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
#include "flat_hash_map.hpp"

namespace cyb
{
    struct ResourceInternal : public Resource::InternalBaseData
    {
        std::vector<uint8_t> data;
        cyb::rhi::Texture texture;
    };

    const rhi::Texture& Resource::GetTexture() const
    {
        assert(m_internalState != nullptr);
        assert(m_internalState->type == ResourceType::Image);
        const ResourceInternal* resourceinternal = (ResourceInternal*)m_internalState.get();
        return resourceinternal->texture;
    }
}

namespace cyb::resourcemanager
{
    std::mutex locker;
    std::unordered_map<uint64_t, std::weak_ptr<ResourceInternal>> resourceCache;
    std::vector<std::string> searchPaths;
    DirectoryWatcher directoryWatcher;
    CVar<bool> cl_assetReloadOnChange{ "cl_assetReloadOnChange", true, CVarFlag::SystemBit, "Auto reload loaded asset on filesystem change" };
    CVar<uint32_t> cl_assetChangeStableDelay{ "cl_assetChangeStableDelay", 200, CVarFlag::SystemBit, "Delay in milliseconds from filesystem change to stable file" };

    /**
     * @brief Callback for directory watcher when a file change is detected.
     */
    static void OnAssetFileChangeEvent(const FileChangeEvent& event)
    {
        if (event.action != cyb::FileChangeAction::Modified)
            return;

        const uint64_t hash = HashString(event.filename);
        locker.lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        locker.unlock();

        // If the file is not loaded, ignore the change.
        if (internalState == nullptr)
            return;

        if (HasFlag(internalState->flags, AssetFlags::IgnoreFilesystemChange))
            return;

        CYB_TRACE("Detected change in loaded asset ({}), reloading...", event.filename);

        // Ignore the return value here since LoadFile() will update the internal
        // state of the already loaded asset.
        auto _ = LoadFile(event.filename, internalState->flags, true);
    }

    void Initialize()
    {
        cl_assetReloadOnChange.RegisterOnChangeCallback([&] (const CVar<bool>& cvar) {
            if (cvar.GetValue())
            {
                // re-add all search paths and start
                for (const auto& path : searchPaths)
                    directoryWatcher.AddDirectory(path, OnAssetFileChangeEvent, true);
                directoryWatcher.Start();
            }
            else
            {
                directoryWatcher.Start();
            }
        });

        cl_assetChangeStableDelay.RegisterOnChangeCallback([&] (const CVar<uint32_t>& cvar) {
            directoryWatcher.SetEnqueueToStableDelay(cvar.GetValue());
        });

        if (cl_assetReloadOnChange.GetValue())
            directoryWatcher.Start();
    }

    void AddSearchPath(const std::string& path)
    {
        std::scoped_lock lock{ locker };
        const std::string slash = (path[path.length() - 1] != '/') ? "/" : "";
        searchPaths.push_back(path + slash);

        if (cl_assetReloadOnChange.GetValue())
            directoryWatcher.AddDirectory(path, OnAssetFileChangeEvent, true);
    }

    std::string FindFile(const std::string& filename)
    {
        for (const auto& basepath : searchPaths)
        {
            std::string filepath = basepath + filename;
            if (std::filesystem::exists(filepath))
                return filepath;
        }

        return filename;
    }

    ResourceType GetTypeFromExtension(std::string_view extension)
    {
        static const ska::flat_hash_map<std::string_view, ResourceType> types{
            std::make_pair("jpg",   ResourceType::Image),
            std::make_pair("jpeg",  ResourceType::Image),
            std::make_pair("png",   ResourceType::Image),
            std::make_pair("dds",   ResourceType::Image),
            std::make_pair("tga",   ResourceType::Image),
            std::make_pair("bmp",   ResourceType::Image),
            std::make_pair("frag",  ResourceType::Shader),
            std::make_pair("vert",  ResourceType::Shader),
            std::make_pair("geom",  ResourceType::Shader),
            std::make_pair("comp",  ResourceType::Shader)
        };

        const auto& it = types.find(extension);
        return (it != types.end()) ? it->second : ResourceType::None;
    }

    std::string_view GetTypeAsString(ResourceType type)
    {
        switch (type)
        {
        case ResourceType::Image:   return "Image";
        case ResourceType::Shader:  return "Shader";
        case ResourceType::Sound:   return "Sound";
        }

        assert(0);
        return "Unknown";
    }

    static bool LoadImageResouce(std::shared_ptr<ResourceInternal> resource)
    {
        const int channels = 4;
        int width, height, bpp;

        const bool flipImage = HasFlag(resource->flags, AssetFlags::ImageFipBit);
        stbi_set_flip_vertically_on_load(flipImage);
        stbi_uc* rawImage = stbi_load_from_memory(resource->data.data(), (int)resource->data.size(), &width, &height, &bpp, channels);
        if (rawImage == nullptr)
        {
            CYB_ERROR("Failed to decode image (filename={0}): {1}", resource->name, stbi_failure_reason());
            stbi_image_free(rawImage);
            return false;
        }

        rhi::TextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.format = rhi::Format::RGBA8_UNORM;

        rhi::SubresourceData data = rhi::SubresourceData::FromDesc(rawImage, desc);
        rhi::GetDevice()->CreateTexture(&desc, &data, &resource->texture);

        stbi_image_free(rawImage);
        return true;
    }

    [[nodiscard]] static Resource LoadInternalState(std::shared_ptr<ResourceInternal> internalState)
    {
        switch (internalState->type)
        {
        case ResourceType::Image:
            if (!LoadImageResouce(internalState))
                return Resource();
            break;

        case ResourceType::Shader:
        case ResourceType::Sound:
        case ResourceType::None:
        default:
            assert(0 && "Unimplemented");
            break;
        }

        if (!HasFlag(internalState->flags, AssetFlags::RetainFiledataBit))
            internalState->data.clear();

        return Resource(internalState);
    }

    Resource LoadFile(const std::string& name, AssetFlags flags, bool force)
    {
        Timer timer;

        // compute hash for the asset and try locate it in the cache
        std::string fixedName = filesystem::FixFilePath(name);
        const uint64_t hash = HashString(fixedName);
        locker.lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        if (internalState != nullptr)
        {
            if (!force)
            {
                // Asset is already loaded
                locker.unlock();
                CYB_TRACE("Grabbed {} asset from cache name={} hash=0x{:x}", GetTypeAsString(internalState->type), fixedName, hash);
                return Resource(internalState);
            } 
            else
            {
                // Force reload of the asset
                internalState->flags = flags;
                internalState->name = fixedName;
                resourceCache[hash] = internalState;
            }
        }
        else
        {
            // Create a new asset
            internalState = std::make_shared<ResourceInternal>();
            internalState->name = fixedName;
            internalState->hash = hash;
            internalState->flags = flags;
            internalState->type = GetTypeFromExtension(filesystem::GetExtension(fixedName));
            if (internalState->type == ResourceType::None)
            {
                locker.unlock();
                CYB_ERROR("Failed to determine resource type (filename={0})", fixedName);
                return Resource();
            }

            resourceCache[hash] = internalState;
        }
        locker.unlock();

        /*
         * NOTE: If we could check if ReadFile is being run on a mechanical hard drive we 
         * could make a special case where the mutex unlocks after file read since starting
         * a lot of file reads on mechanical drive async will hurt it's performance.
         */
        if (!filesystem::ReadFile(FindFile(name), internalState->data))
            return Resource();

        Resource loadedAsset = LoadInternalState(internalState);
        CYB_TRACE("Loaded {} asset name={} hash=0x{:x} in {:.2f}ms", GetTypeAsString(internalState->type), fixedName, hash, timer.ElapsedMilliseconds());

        return loadedAsset;
    }

    void Clear()
    {
        std::scoped_lock lock{ locker };
        resourceCache.clear();
    }
};
