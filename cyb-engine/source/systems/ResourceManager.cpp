#include "core/logger.h"
#include "Core/Helper.h"
#include "Core/Timer.h"
#include "Graphics/Renderer.h"

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

#include <unordered_map>
#include <mutex>

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
        {
            internal_state = std::make_shared<ResourceInternal>();
        }

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->data = data;
    }

    void Resource::SetFileData(std::vector<uint8_t>&& data)
    {
        if (internal_state == nullptr)
        {
            internal_state = std::make_shared<ResourceInternal>();
        }

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->data = data;
    }

    void Resource::SetTexture(const graphics::Texture& texture)
    {
        if (internal_state == nullptr)
        {
            internal_state = std::make_shared<ResourceInternal>();
        }

        ResourceInternal* resourceinternal = (ResourceInternal*)internal_state.get();
        resourceinternal->texture = texture;
    }
}

namespace cyb::resourcemanager
{
    std::mutex locker;
    std::unordered_map<std::string, std::weak_ptr<ResourceInternal>> resource_cache;
    Mode mode = Mode::DISCARD_FILEDATA_AFTER_LOAD;

    enum class DataType
    {
        IMAGE,
        SOUND,
    };

    std::unordered_map<std::string, DataType> types = 
    {
        std::make_pair("JPG", DataType::IMAGE),
        std::make_pair("PNG", DataType::IMAGE),
        std::make_pair("DDS", DataType::IMAGE),
        std::make_pair("TGA", DataType::IMAGE)
    };

    Resource Load(
        const std::string& name,
        uint32_t flags,
        const uint8_t* filedata,
        size_t filesize)
    {
        Timer timer;
        timer.Record();

        if (mode == Mode::DISCARD_FILEDATA_AFTER_LOAD)
        {
            flags &= ~Flags::IMPORT_RETAIN_FILEDATA;
        }

        // Check if we have allready loaded resource or we need to create it
        locker.lock();
        std::shared_ptr<ResourceInternal> resource = resource_cache[name].lock();
        if (resource != nullptr)
        {
            locker.unlock();
            Resource ret;
            ret.internal_state = resource;
            return ret;
        }
        
        resource = std::make_shared<ResourceInternal>();    
        resource_cache[name] = resource;
        locker.unlock();

        // Load filedata if none was appointed
        if (filedata == nullptr || filesize == 0)
        {
            if (!helper::FileRead(name, resource->data))
            {
                resource.reset();
                return Resource();
            }

            filedata = resource->data.data();
            filesize = resource->data.size();
        }

        std::string ext = helper::ToUpper(helper::GetExtensionFromFileName(name));
        DataType type;

        // dynamic type selection:
        {
            auto it = types.find(ext);
            if (it == types.end())
            {
                return Resource();
            }
                
            type = it->second;
        }

        switch (type)
        {
        case DataType::IMAGE:
        {
            const int channels = 4;
            int width, height, bpp;

            bool flip_image = !(flags & IMPORT_FLIP_IMAGE);
            stbi_set_flip_vertically_on_load(flip_image);
            stbi_uc* raw_image = stbi_load_from_memory(filedata, (int)filesize, &width, &height, &bpp, channels);
            if (raw_image == nullptr)
            {
                CYB_ERROR("Failed to decode image (filename={0}): {1}", name, stbi_failure_reason());
                stbi_image_free(raw_image);
                return Resource();
            }

            graphics::TextureDesc desc;
            desc.width = width;
            desc.height = height;
            desc.format = graphics::Format::R8G8B8A8_UNORM;
            desc.bind_flags = graphics::BindFlag::SHADER_RESOURCE;
            desc.mip_levels = 1;     // Generate full mip chain at runtime

            graphics::SubresourceData subresource_data;
            subresource_data.mem = raw_image;
            subresource_data.row_pitch = width * channels;
            
            renderer::GetDevice()->CreateTexture(&desc, &subresource_data, &resource->texture);
            stbi_image_free(raw_image);
        } break;
        };

        if (!resource->data.empty() && (flags & IMPORT_RETAIN_FILEDATA) == 0)
        {
            resource->data.clear();
        }

        CYB_TRACE("Loaded resource (filename={0}) in {1:.2f}ms", name, timer.ElapsedMilliseconds());
        
        Resource ret;
        ret.internal_state = resource;
        return ret;
    }

    void Clear()
    {
        locker.lock();
        resource_cache.clear();
        locker.unlock();
    }
};
