#include <mutex>
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/timer.h"
#include "core/hash.h"
#include "graphics/renderer.h"
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
#include <filesystem>

namespace cyb {
    struct ResourceInternal : public Resource::InternalBaseData {
        std::vector<uint8_t> data;
        cyb::graphics::Texture texture;
    };

    const graphics::Texture& Resource::GetTexture() const {
        assert(m_internalState != nullptr);
        assert(m_internalState->type == ResourceType::Image);
        const ResourceInternal* resourceinternal = (ResourceInternal*)m_internalState.get();
        return resourceinternal->texture;
    }
}

namespace cyb::resourcemanager {

    std::mutex locker;
    std::unordered_map<AssetHash, std::weak_ptr<ResourceInternal>> resourceCache;
    std::vector<std::string> searchPaths;

    static const ska::flat_hash_map<std::string_view, ResourceType> types =  {
        std::make_pair("jpg",  ResourceType::Image),
        std::make_pair("jpeg", ResourceType::Image),
        std::make_pair("png",  ResourceType::Image),
        std::make_pair("dds",  ResourceType::Image),
        std::make_pair("tga",  ResourceType::Image),
        std::make_pair("bmp",  ResourceType::Image),
        std::make_pair("frag",  ResourceType::Shader),
        std::make_pair("vert",  ResourceType::Shader),
        std::make_pair("geom",  ResourceType::Shader),
        std::make_pair("comp",  ResourceType::Shader)
    };

    void AddSearchPath(const std::string& path) {
        std::scoped_lock l(locker);
        const std::string slash = (path[path.length() - 1] != '/') ? "/" : "";
        searchPaths.push_back(path + slash);
    }

    std::string FindFile(const std::string& filename) {
        for (const auto& basepath : searchPaths) {
            std::string filepath = basepath + filename;
            if (std::filesystem::exists(filepath))
                return filepath;
        }

        return filename;
    }

    const char* GetTypeAsString(ResourceType type) {
        switch (type) {
        case ResourceType::None:    return "None";
        case ResourceType::Image:   return "Image";
        case ResourceType::Sound:   return "Sound";
        }

        assert(0);
        return "None";
    }

    static [[nodiscard]] Resource Load(std::shared_ptr<ResourceInternal> internalState) {
        switch (internalState->type) {
        case ResourceType::Image: {
            const int channels = 4;
            int width, height, bpp;

            const bool flipImage = HasFlag(internalState->flags, AssetFlags::ImageFipBit);
            stbi_set_flip_vertically_on_load(flipImage);
            stbi_uc* rawImage = stbi_load_from_memory(internalState->data.data(), (int)internalState->data.size(), &width, &height, &bpp, channels);
            if (rawImage == nullptr) {
                CYB_ERROR("Failed to decode image (filename={0}): {1}", internalState->name, stbi_failure_reason());
                stbi_image_free(rawImage);
                return Resource();
            }

            graphics::TextureDesc desc;
            desc.width = width;
            desc.height = height;
            desc.format = graphics::Format::R8G8B8A8_Unorm;
            desc.bindFlags = graphics::BindFlags::ShaderResourceBit;
            desc.mipLevels = 1;     // generate full mip chain at runtime

            graphics::SubresourceData subresourceData;
            subresourceData.mem = rawImage;
            subresourceData.rowPitch = width * channels;

            graphics::GetDevice()->CreateTexture(&desc, &subresourceData, &internalState->texture);
            stbi_image_free(rawImage);
        } break;

        case ResourceType::Sound:
        case ResourceType::None:
            assert(0 && "Unimplemented");
            break;
        }

        if (!HasFlag(internalState->flags, AssetFlags::RetainFiledataBit))
            internalState->data.clear();

        return Resource(internalState);
    }

    Resource LoadFile(const std::string& name, AssetFlags flags) {
        Timer timer;

        // dynamic type selection
        const auto& typeIt = types.find(filesystem::GetExtension(name));
        if (typeIt == types.end()) {
            CYB_ERROR("Failed to determine resource type (filename={0})", name);
            return Resource();
        }

        // compute hash for the asset and try locate it in the cache
        const AssetHash hash = hash::String(name);
        locker.lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        if (internalState != nullptr) {
            locker.unlock();
            CYB_TRACE("Grabbed {} asset from cache (name={})", GetTypeAsString(internalState->type), name);
            return Resource(internalState);
        }
        
        CYB_TRACE("Creating new asset, name={} hash=0x{:x}, type={}", name, hash, GetTypeAsString(typeIt->second));
        internalState = std::make_shared<ResourceInternal>();
        internalState->name = name;
        internalState->hash = hash;
        internalState->type = typeIt->second;
        internalState->flags = flags;
        resourceCache[hash] = internalState;
        locker.unlock();

        if (!filesystem::ReadFile(FindFile(name), internalState->data))
            return Resource();

        Resource loadedAsset = Load(internalState);
        CYB_INFO("Loaded {} resource {} in {:.2f}ms", GetTypeAsString(internalState->type), internalState->name, timer.ElapsedMilliseconds());

        return loadedAsset;
    }

    void Clear() {
        std::scoped_lock lock(locker);
        resourceCache.clear();
    }
};
