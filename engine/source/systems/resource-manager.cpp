#include "core/logger.h"
#include "core/filesystem.h"
#include "core/timer.h"
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
        cyb::graphics::Texture texture;
        std::vector<uint8_t> data;
    };

    const std::vector<uint8_t>& Resource::GetFileData() const
    {
        const ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        return resourceinternal->data;
    }

    const graphics::Texture& Resource::GetTexture() const
    {
        const ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        return resourceinternal->texture;
    }

    void Resource::SetFileData(const std::vector<uint8_t>& data)
    {
        if (internal_state == nullptr)
            internal_state = std::make_shared<ResourceInternal>();

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->data = data;
    }

    void Resource::SetFileData(std::vector<uint8_t>&& data)
    {
        if (internal_state == nullptr)
            internal_state = std::make_shared<ResourceInternal>();

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->data = data;
    }

    void Resource::SetTexture(const graphics::Texture& texture)
    {
        if (internal_state == nullptr)
            internal_state = std::make_shared<ResourceInternal>();

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->texture = texture;
    }
}

namespace cyb::resourcemanager
{
    std::mutex locker;
    std::unordered_map<std::string, std::weak_ptr<ResourceInternal>> resourceCache;
    Mode mode = Mode::DiscardFiledataAfterLoad;

    enum class DataType
    {
        Unknown,
        Image,
        Sound,
    };

    DataType GetResourceTypeByExtension(const std::string& extension)
    {
        static const std::unordered_map<std::string, DataType> types = {
            std::make_pair("jpg", DataType::Image),
            std::make_pair("png", DataType::Image),
            std::make_pair("dds", DataType::Image),
            std::make_pair("tga", DataType::Image)
        };

        auto it = types.find(extension);
        if (it == types.end())
            return DataType::Unknown;

        return it->second;
    }

    Resource Load(
        const std::string& name,
        LoadFlags flags,
        const uint8_t* filedata,
        size_t filesize)
    {
        Timer timer;
        timer.Record();

        if (mode == Mode::DiscardFiledataAfterLoad)
            flags &= ~LoadFlags::RetainFiledataBit;

        // Check if we have allready loaded resource or we need to create it
        locker.lock();
        std::shared_ptr<ResourceInternal> resource = resourceCache[name].lock();
        if (resource != nullptr)
        {
            locker.unlock();
            Resource ret;
            ret.internal_state = resource;
            return ret;
        }
        
        resource = std::make_shared<ResourceInternal>();    
        resourceCache[name] = resource;
        locker.unlock();

        // Load filedata if none was appointed
        if (filedata == nullptr || filesize == 0)
        {
            if (!filesystem::ReadFile(name, resource->data))
            {
                resource.reset();
                return Resource();
            }

            filedata = resource->data.data();
            filesize = resource->data.size();
        }

        const std::string extension = filesystem::GetFileExtension(name);
        const DataType type = GetResourceTypeByExtension(extension);
        if (type == DataType::Unknown)
        {
            CYB_ERROR("Failed to determine resource type (filename={0})", name);
            return Resource();
        }

        switch (type)
        {
        case DataType::Image:
        {
            const int channels = 4;
            int width, height, bpp;

            const bool flipImage = !HasFlag(flags, LoadFlags::FlipImageBit);
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
        };

        if (!resource->data.empty() && !HasFlag(flags, LoadFlags::RetainFiledataBit))
            resource->data.clear();

        CYB_INFO("Loaded resource (filename={0}) in {1:.2f}ms", name, timer.ElapsedMilliseconds());
        
        Resource ret;
        ret.internal_state = resource;
        return ret;
    }

    void Clear()
    {
        std::scoped_lock<std::mutex> lock(locker);
        resourceCache.clear();
    }
};
