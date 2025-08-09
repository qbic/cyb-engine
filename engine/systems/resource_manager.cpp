#include <filesystem>
#include "core/cvar.h"
#include "core/directory_watcher.h"
#include "core/filesystem.h"
#include "core/hash.h"
#include "core/logger.h"
#include "core/mutex.h"
#include "core/timer.h"
#include "graphics/device.h"
#include "systems/resource_manager.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
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
    Mutex locker;
    std::unordered_map<uint64_t, std::weak_ptr<ResourceInternal>> resourceCache;
    std::vector<std::string> searchPaths;
    DirectoryWatcher directoryWatcher;
    CVar<bool> assetReloadOnChange("assetReloadOnChange", true, CVarFlag::SystemBit, "Auto reload loaded asset on filesystem change");
    CVar<uint32_t> assetFileWatcherStableDelay("assetFileWatcherStableDelay", 200, CVarFlag::SystemBit, "Delay in milliseconds from filesystem change to stable file");

    static const ska::flat_hash_map<std::string_view, ResourceType> types = {
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

    static void OnAssetFileChangeEvent(const FileChangeEvent& event)
    {
        if (event.action != cyb::FileChangeAction::Modified)
            return;

        const uint64_t hash = hash::String(event.filename);
        locker.Lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        locker.Unlock();
        if (internalState != nullptr)
        {
            if (HasFlag(internalState->flags, AssetFlags::IgnoreFilesystemChange))
                return;

            CYB_TRACE("Detected change in loaded asset ({}), reloading...", event.filename);

            // Return value is ignored here sence LoadFile() will update the inrernal_state in cache.
            auto _ = LoadFile(event.filename, internalState->flags, true);
        }
    }

    void Initialize()
    {
        assetReloadOnChange.RegisterOnChangeCallback([&] (const CVar<bool>& cvar) {
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

        assetFileWatcherStableDelay.RegisterOnChangeCallback([&] (const CVar<uint32_t>& cvar) {
            directoryWatcher.SetEnqueueToStableDelay(cvar.GetValue());
        });

        if (assetReloadOnChange.GetValue())
            directoryWatcher.Start();
    }

    void AddSearchPath(const std::string& path)
    {
        ScopedMutex lck(locker);
        const std::string slash = (path[path.length() - 1] != '/') ? "/" : "";
        searchPaths.push_back(path + slash);

        if (assetReloadOnChange.GetValue())
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

    bool GetAssetTypeFromFilename(ResourceType* type, const std::string& filename)
    {
        const auto& it = types.find(filesystem::GetExtension(filename));
        if (it == types.end())
            return false;

        if (type != nullptr)
            *type = it->second;
        return true;
    }

    const char* GetTypeAsString(ResourceType type)
    {
        switch (type)
        {
        case ResourceType::None:    return "None";
        case ResourceType::Image:   return "Image";
        case ResourceType::Shader:  return "Shader";
        case ResourceType::Sound:   return "Sound";
        }

        assert(0);
        return "None";
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
        const uint64_t hash = hash::String(fixedName);
        locker.Lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        if (internalState != nullptr && force == false)
        {
            locker.Unlock();
            CYB_TRACE("Grabbed {} asset from cache name={} hash=0x{:x}", GetTypeAsString(internalState->type), fixedName, hash);
            return Resource(internalState);
        }
        else
        {
            internalState = std::make_shared<ResourceInternal>();
            internalState->name = fixedName;
            internalState->hash = hash;
            internalState->flags = flags;
        }

        if (!GetAssetTypeFromFilename(&internalState->type, fixedName))
        {
            locker.Unlock();
            CYB_ERROR("Failed to determine resource type (filename={0})", fixedName);
            return Resource();
        }

        resourceCache[hash] = internalState;
        locker.Unlock();

        /*
         * NOTE: If we could check if ReadFile is being run on a machanical hard drive we 
         * could make a special case where the mutex unlocks after file read sence starting
         * alot of file reads on mechanical drive async will hurt it's performance.
         */
        if (!filesystem::ReadFile(FindFile(name), internalState->data))
            return Resource();

        Resource loadedAsset = LoadInternalState(internalState);
        CYB_TRACE("Loaded {} asset name={} hash=0x{:x} in {:.2f}ms", GetTypeAsString(internalState->type), fixedName, hash, timer.ElapsedMilliseconds());

        return loadedAsset;
    }

    void Clear()
    {
        ScopedMutex lck(locker);
        resourceCache.clear();
    }
};
