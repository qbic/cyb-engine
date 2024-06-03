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
    std::unordered_map<Resource::HashType, std::weak_ptr<ResourceInternal>> resourceCache;
    std::vector<std::string> searchPaths;

    static const std::unordered_map<std::string, ResourceType> types =  {
        std::make_pair("jpg",  ResourceType::Image),
        std::make_pair("jpeg", ResourceType::Image),
        std::make_pair("png",  ResourceType::Image),
        std::make_pair("dds",  ResourceType::Image),
        std::make_pair("tga",  ResourceType::Image),
        std::make_pair("bmp",  ResourceType::Image)
    };

    void AddSearchPath(const std::string& path) {
        std::scoped_lock l(locker);
        searchPaths.push_back(path);
    }

    std::string FindFile(const std::string& filename) {
        for (const auto& basepath : searchPaths) {
            std::string filepath = basepath + filename;
            if (std::filesystem::exists(filepath))
                return filepath;
        }

        return filename;
    }

    const char* GetResourceTypeString(ResourceType type) {
        switch (type) {
        case ResourceType::None:    return "None";
        case ResourceType::Image:   return "Image";
        case ResourceType::Sound:   return "Sound";
        }

        assert(0);
        return "None";
    }

    static Resource Load(std::shared_ptr<ResourceInternal> internalState) {
        switch (internalState->type) {
        case ResourceType::Image: {
            const int channels = 4;
            int width, height, bpp;

            const bool flipImage = HasFlag(internalState->flags, Resource::Flags::ImageFipBit);
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

        if (!HasFlag(internalState->flags, Resource::Flags::RetainFiledataBit))
            internalState->data.clear();

        return Resource(internalState);
    }

    Resource LoadFile(const std::string& name, Resource::Flags flags) {
        Timer timer;

        // dynamic type selection
        const auto& typeIt = types.find(filesystem::GetExtension(name));
        if (typeIt == types.end()) {
            CYB_ERROR("Failed to determine resource type (filename={0})", name);
            return Resource();
        }

        // compute hash for the asset and try locate it in the cache
        const Resource::HashType hash = hash::String(name);
        locker.lock();
        std::shared_ptr<ResourceInternal> internalState = resourceCache[hash].lock();
        if (internalState != nullptr) {
            locker.unlock();
            CYB_TRACE("Grabbed {} asset from cache (name={})", GetResourceTypeString(internalState->type), name);
            return Resource(internalState);
        }
        
        CYB_TRACE("Creating new asset, name={} hash=0x{:x}, type={}", name, hash, GetResourceTypeString(typeIt->second));
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
        CYB_INFO("Loaded {} resource {} in {:.2f}ms", GetResourceTypeString(internalState->type), internalState->name, timer.ElapsedMilliseconds());

        return loadedAsset;
    }

    void Clear() {
        std::scoped_lock lock(locker);
        resourceCache.clear();
    }
};
