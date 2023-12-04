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

namespace cyb
{
    struct ResourceInternal
    {
        std::string name;
        uint64_t nameHash;
        std::vector<uint8_t> data;
        cyb::graphics::Texture texture;
    };

    const graphics::Texture& Resource::GetTexture() const
    {
        const ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        return resourceinternal->texture;
    }
}

namespace cyb::resourcemanager
{
    std::mutex locker;
    std::unordered_map<uint64_t, std::weak_ptr<ResourceInternal>> resourceCache;
    Mode mode = Mode::DiscardFiledataAfterLoad;

    enum class DataType
    {
        Image,
        Sound,
    };

    static const std::unordered_map<std::string, DataType> types = {
        std::make_pair("jpg",  DataType::Image),
        std::make_pair("jpeg", DataType::Image),
        std::make_pair("png",  DataType::Image),
        std::make_pair("dds",  DataType::Image),
        std::make_pair("tga",  DataType::Image),
        std::make_pair("bmp",  DataType::Image)
    };

    Resource Load(
        const std::string& name,
        Flags flags,
        const uint8_t* filedata,
        size_t filesize)
    {
        Timer timer;

        if (mode == Mode::DiscardFiledataAfterLoad)
        {
            flags &= ~Flags::RetainFiledataBit;
        }

        // Check if we have allready loaded resource or we need to create it
        const uint64_t hash = hash::String(name);
        locker.lock();
        std::shared_ptr<ResourceInternal> resource = resourceCache[hash].lock();
        if (resource != nullptr)
        {
            locker.unlock();
            return Resource{ resource };
        }
        
        resource = std::make_shared<ResourceInternal>();
        resource->name = name;
        resource->nameHash = hash;
        resourceCache[hash] = resource;
        locker.unlock();

        // Load filedata if none was appointed
        if (filedata == nullptr || filesize == 0)
        {
            if (!filesystem::ReadFile(name, resource->data))
            {
                return Resource();
            }

            filedata = resource->data.data();
            filesize = resource->data.size();
        }

        // Dynamic type selection
        const auto& typeIt = types.find(filesystem::GetExtension(name));
        if (typeIt == types.end())
        {
            CYB_ERROR("Failed to determine resource type (filename={0})", name);
            return Resource();
        }

        switch (typeIt->second)
        {
        case DataType::Image:
        {
            const int channels = 4;
            int width, height, bpp;

            const bool flipImage = HasFlag(flags, Flags::FlipImageBit);
            stbi_set_flip_vertically_on_load(flipImage);
            stbi_uc* rawImage = stbi_load_from_memory(filedata, (int)filesize, &width, &height, &bpp, channels);
            if (rawImage == nullptr)
            {
                CYB_ERROR("Failed to decode image (filename={0}): {1}", name, stbi_failure_reason());
                stbi_image_free(rawImage);
                return Resource();
            }

            graphics::TextureDesc desc;
            desc.width = width;
            desc.height = height;
            desc.format = graphics::Format::R8G8B8A8_Unorm;
            desc.bindFlags = graphics::BindFlags::ShaderResourceBit;
            desc.mipLevels = 1;     // Generate full mip chain at runtime

            graphics::SubresourceData subresourceData;
            subresourceData.mem = rawImage;
            subresourceData.rowPitch = width * channels;
            
            graphics::GetDevice()->CreateTexture(&desc, &subresourceData, &resource->texture);
            stbi_image_free(rawImage);
        } break;
        }

        if (!HasFlag(flags, Flags::RetainFiledataBit))
        {
            resource->data.clear();
        }

        CYB_INFO("Loaded resource (filename={0}) in {1:.2f}ms", name, timer.ElapsedMilliseconds());
        return Resource{ resource };
    }

    void Clear()
    {
        std::scoped_lock lock(locker);
        resourceCache.clear();
    }
};
