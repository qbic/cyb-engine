#include <unordered_set>
#include <unordered_map>
#include "core/logger.h"
#include "core/platform.h"
#include "core/hash.h"
#include "graphics/device_vulkan.h"
#include "volk.h"
#include "spirv_reflect.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif  // __clang__
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__

#ifndef _DEBUG
#define VK_ASSERT(cond, fname) {if (!cond) { FatalError(std::format("Vulkan error: {} failed with code {} ({}:{})", fname, (int)res, __FILE__, __LINE__)); } }
#else
#define VK_ASSERT(cond, fname) {if (!cond) { CYB_ERROR("Vulkan error: {} failed with code {} ({}:{})", fname, (int)res, __FILE__, __LINE__); assert(cond); } }
#endif
#define VK_CHECK(call) [&]() {VkResult res = call; VK_ASSERT((res >= VK_SUCCESS), #call); return res;}()

namespace cyb::rhi::vulkan_internal
{
    static constexpr uint64_t timeoutValue = 2000000000ull; // 2 seconds

    static constexpr VkFormat ConvertFormat(Format value)
    {
        switch (value)
        {
        case Format::Unknown:               return VK_FORMAT_UNDEFINED;
        case Format::RGBA32_FLOAT:          return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::RG32_FLOAT:            return VK_FORMAT_R32G32_SFLOAT;
        case Format::RGBA8_UNORM:           return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_UINT:            return VK_FORMAT_R8G8B8A8_UINT;
        case Format::RG16_FLOAT:            return VK_FORMAT_R16G16_SFLOAT;
        case Format::D32:                   return VK_FORMAT_D32_SFLOAT;
        case Format::D24S8:                 return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32S8:                 return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::R32_FLOAT:             return VK_FORMAT_R32_SFLOAT;
        case Format::R16_FLOAT:             return VK_FORMAT_R16_SFLOAT;
        case Format::R8_UNORM:              return VK_FORMAT_R8_UNORM;
        case Format::BGRA8_UNORM:           return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::RGB32_FLOAT:           return VK_FORMAT_R32G32B32_SFLOAT;
        }

        assert(0);
        return VK_FORMAT_UNDEFINED;
    }

    static constexpr VkImageType ConvertTextureType(TextureType dimension)
    {
        switch (dimension)
        {
        case TextureType::Texture1D:   return VK_IMAGE_TYPE_1D;
        case TextureType::Texture2D:   return VK_IMAGE_TYPE_2D;
        case TextureType::Texture3D:   return VK_IMAGE_TYPE_3D;
        }

        assert(0);
        return VK_IMAGE_TYPE_MAX_ENUM;
    }

    static constexpr VkSamplerAddressMode ConvertSamplerAddressMode(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Wrap:      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::Mirror:    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::Clamp:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }

        assert(0);
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };

    static constexpr VkComponentSwizzle ConvertComponentSwizzle(ComponentSwizzle swizzle)
    {
        switch (swizzle)
        {
        case ComponentSwizzle::Zero:        return VK_COMPONENT_SWIZZLE_ZERO;
        case ComponentSwizzle::One:         return VK_COMPONENT_SWIZZLE_ONE;
        case ComponentSwizzle::R:           return VK_COMPONENT_SWIZZLE_R;
        case ComponentSwizzle::G:           return VK_COMPONENT_SWIZZLE_G;
        case ComponentSwizzle::B:           return VK_COMPONENT_SWIZZLE_B;
        case ComponentSwizzle::A:           return VK_COMPONENT_SWIZZLE_A;
        }

        assert(0);
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    static constexpr VkCompareOp ConvertComparisonFunc(ComparisonFunc value)
    {
        switch (value)
        {
        case ComparisonFunc::Never:         return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:          return VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:         return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessOrEqual:   return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:       return VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:      return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Allways:       return VK_COMPARE_OP_ALWAYS;
        }

        assert(0);
        return VK_COMPARE_OP_NEVER;
    }

    static constexpr VkStencilOp ConvertStencilOp(StencilOp value)
    {
        switch (value)
        {
        case StencilOp::Keep:               return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero:               return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace:            return VK_STENCIL_OP_REPLACE;
        case StencilOp::IncrementClamp:     return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecrementClamp:     return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::Invert:             return VK_STENCIL_OP_INVERT;
        case StencilOp::Increment:          return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::Decrement:          return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }

        assert(0);
        return VK_STENCIL_OP_KEEP;
    }

    static constexpr VkAttachmentLoadOp ConvertLoadOp(RenderPassImage::LoadOp loadOp)
    {
        switch (loadOp)
        {
        case RenderPassImage::LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RenderPassImage::LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RenderPassImage::LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }

        assert(0);
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    static constexpr VkAttachmentStoreOp ConvertStoreOp(RenderPassImage::StoreOp storeOp)
    {
        switch (storeOp)
        {
        case RenderPassImage::StoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case RenderPassImage::StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }

        assert(0);
        return VK_ATTACHMENT_STORE_OP_STORE;
    }

    static VkImageLayout ConvertImageLayout(ResourceStates state)
    {
        if (state == ResourceStates::Unknown)
            return VK_IMAGE_LAYOUT_UNDEFINED;

        // Prioritize depth write (depth-stencil write)
        if (HasFlag(state, ResourceStates::DepthWriteBit))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Depth read-only (depth-stencil read)
        if (HasFlag(state, ResourceStates::DepthReadBit))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        if (HasFlag(state, ResourceStates::RenderTargetBit))
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (HasFlag(state, ResourceStates::UnorderedAccessBit))
            return VK_IMAGE_LAYOUT_GENERAL;

        if (HasFlag(state, ResourceStates::ShaderResourceBit))
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (HasFlag(state, ResourceStates::CopySourceBit))
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (HasFlag(state, ResourceStates::CopyDestBit))
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        // fallback to GENERAL if unknown or multiple conflicting bits
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    static VkPipelineStageFlags ConvertStageMask(ResourceStates state)
    {
        VkPipelineStageFlags stages = 0;

        if (HasFlag(state, ResourceStates::VertexBufferBit) ||
            HasFlag(state, ResourceStates::IndexBufferBit))
            stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

        if (HasFlag(state, ResourceStates::ConstantBufferBit) ||
            HasFlag(state, ResourceStates::ShaderResourceBit) ||
            HasFlag(state, ResourceStates::UnorderedAccessBit))
            stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        if (HasFlag(state, ResourceStates::RenderTargetBit))
            stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if (HasFlag(state, ResourceStates::DepthReadBit) ||
            HasFlag(state, ResourceStates::DepthWriteBit))
            stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

        if (HasFlag(state, ResourceStates::CopySourceBit) ||
            HasFlag(state, ResourceStates::CopyDestBit))
            stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;

        return stages;
    }

    struct Buffer_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VmaAllocation allocation = nullptr;
        VkBuffer resource = VK_NULL_HANDLE;

        ~Buffer_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.Lock();
            uint64_t framecount = allocationhandler->framecount;
            allocationhandler->destroyer_buffers.push_back(std::make_pair(std::make_pair(resource, allocation), framecount));
            allocationhandler->destroylocker.Unlock();
        }
    };

    struct Query_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkQueryPool pool = VK_NULL_HANDLE;

        ~Query_Vulkan()
        {
            if (allocationhandler == nullptr)
                return;
            allocationhandler->destroylocker.Lock();
            uint64_t framecount = allocationhandler->framecount;
            if (pool) allocationhandler->destroyer_querypools.push_back(std::make_pair(pool, framecount));
            allocationhandler->destroylocker.Unlock();
        }
    };

    struct Texture_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationHandler;
        VmaAllocation allocation = nullptr;
        VkImage resource = VK_NULL_HANDLE;

        struct TextureSubresource
        {
            VkImageView imageView = VK_NULL_HANDLE;
            uint32_t firstMip = 0;
            uint32_t mipCount = 0;
            uint32_t firstSlice = 0;
            uint32_t sliceCount = 0;

            bool IsValid() const { return imageView != VK_NULL_HANDLE; }
        };

        TextureSubresource srv;
        TextureSubresource rtv;
        TextureSubresource dsv;

        ~Texture_Vulkan()
        {
            assert(allocationHandler != nullptr);
            allocationHandler->destroylocker.Lock();
            uint64_t framecount = allocationHandler->framecount;
            if (resource) allocationHandler->destroyer_images.push_back(std::make_pair(std::make_pair(resource, allocation), framecount));
            if (srv.IsValid()) allocationHandler->destroyer_imageviews.push_back(std::make_pair(srv.imageView, framecount));
            if (rtv.IsValid()) allocationHandler->destroyer_imageviews.push_back(std::make_pair(rtv.imageView, framecount));
            if (dsv.IsValid()) allocationHandler->destroyer_imageviews.push_back(std::make_pair(dsv.imageView, framecount));
            allocationHandler->destroylocker.Unlock();
        }
    };

    struct Shader_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkShaderModule shadermodule = VK_NULL_HANDLE;
        VkPipelineShaderStageCreateInfo stageInfo = {};

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        VkDeviceSize uniform_buffer_sizes[DESCRIPTORBINDER_CBV_COUNT] = {};
        std::vector<uint32_t> uniform_buffer_dynamic_slots;
        std::vector<VkImageViewType> imageViewTypes;

        VkPushConstantRange pushconstants = {};

        ~Shader_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.Lock();
            uint64_t framecount = allocationhandler->framecount;
            if (shadermodule) allocationhandler->destroyer_shadermodules.push_back(std::make_pair(shadermodule, framecount));
            allocationhandler->destroylocker.Unlock();
        }
    };

    struct Sampler_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkSampler resource = VK_NULL_HANDLE;

        ~Sampler_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.Lock();
            uint64_t framecount = allocationhandler->framecount;
            if (resource) allocationhandler->destroyer_samplers.push_back(std::make_pair(resource, framecount));
            allocationhandler->destroylocker.Unlock();
        }
    };

    struct PipelineState_Vulkan
    {
        VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };              // no lifetime management here
        VkDescriptorSetLayout descriptorset_layout{ VK_NULL_HANDLE };   // no lifetime management here

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        std::vector<VkImageViewType> imageViewTypes;

        VkPushConstantRange pushconstants{};

        VkDeviceSize uniform_buffer_sizes[DESCRIPTORBINDER_CBV_COUNT]{};
        std::vector<uint32_t> uniform_buffer_dynamic_slots;

        size_t binding_hash{ 0 };

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        VkPipelineShaderStageCreateInfo shaderStages[Numerical(ShaderType::Count)]{};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        VkPipelineRasterizationDepthClipStateCreateInfoEXT depthclip{};
        VkViewport viewport{};
        VkRect2D scissor{};
        VkPipelineViewportStateCreateInfo viewportState{};
        VkPipelineDepthStencilStateCreateInfo depthstencil{};
    };

    struct Swapchain_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent = {};
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        VkSurfaceKHR surface = VK_NULL_HANDLE;

        uint32_t swapchainImageIndex = 0;
        uint32_t swapchainAcquireSemaphoreIndex = 0;
        std::vector<VkSemaphore> swapchainAcquireSemaphores;
        VkSemaphore swapchainReleaseSemaphore = VK_NULL_HANDLE;

        SwapchainDesc desc;
        Mutex locker;

        ~Swapchain_Vulkan()
        {
            if (allocationhandler == nullptr)
                return;
            allocationhandler->destroylocker.Lock();
            uint64_t framecount = allocationhandler->framecount;

            for (size_t i = 0; i < swapchainImages.size(); ++i)
            {
                allocationhandler->destroyer_imageviews.push_back(std::make_pair(swapchainImageViews[i], framecount));
                allocationhandler->destroyer_semaphores.push_back(std::make_pair(swapchainAcquireSemaphores[i], framecount));
            }

            allocationhandler->destroyer_swapchains.push_back(std::make_pair(swapchain, framecount));
            allocationhandler->destroyer_surfaces.push_back(std::make_pair(surface, framecount));
            allocationhandler->destroyer_semaphores.push_back(std::make_pair(swapchainReleaseSemaphore, framecount));
            allocationhandler->destroylocker.Unlock();
        }
    };

    static Buffer_Vulkan* ToInternal(const GPUBuffer* param)
    {
        return static_cast<Buffer_Vulkan*>(param->internal_state.get());
    }

    static Texture_Vulkan* ToInternal(const Texture* param)
    {
        return static_cast<Texture_Vulkan*>(param->internal_state.get());
    }

    static Shader_Vulkan* ToInternal(const Shader* param)
    {
        return static_cast<Shader_Vulkan*>(param->internal_state.get());
    }

    static Sampler_Vulkan* ToInternal(const Sampler* param)
    {
        return static_cast<Sampler_Vulkan*>(param->internal_state.get());
    }

    static PipelineState_Vulkan* ToInternal(const PipelineState* param)
    {
        return static_cast<PipelineState_Vulkan*>(param->internal_state.get());
    }

    static Swapchain_Vulkan* ToInternal(const Swapchain* param)
    {
        return static_cast<Swapchain_Vulkan*>(param->internal_state.get());
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        CYB_WARNING("Vulkan {0}", callbackData->pMessage);
        CYB_DEBUGBREAK();
        return VK_FALSE;
    }

    static bool CreateSwapchainInternal(
        Swapchain_Vulkan* internal_state,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationHandler)
    {
        VkSurfaceCapabilitiesKHR capabilities = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, internal_state->surface, &capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, internal_state->surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, internal_state->surface, &formatCount, formats.data());

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, internal_state->surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, internal_state->surface, &presentModeCount, presentModes.data());

        VkSurfaceFormatKHR surfaceFormat = {};
        surfaceFormat.format = ConvertFormat(internal_state->desc.format);
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        bool valid = false;

        for (const auto& format : formats)
        {
            if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                continue;
            if (format.format == surfaceFormat.format)
            {
                surfaceFormat = format;
                valid = true;
                break;
            }
        }
        if (!valid)
        {
            surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
            surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }

        if (capabilities.currentExtent.width != 0xFFFFFFFF &&
            capabilities.currentExtent.height != 0xFFFFFFFF)
        {
            internal_state->swapchainExtent = capabilities.currentExtent;
        }
        else
        {
            internal_state->swapchainExtent = { internal_state->desc.width, internal_state->desc.height };
            internal_state->swapchainExtent.width = std::clamp(internal_state->swapchainExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            internal_state->swapchainExtent.height = std::clamp(internal_state->swapchainExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = std::max(internal_state->desc.bufferCount, capabilities.minImageCount);
        if ((capabilities.maxImageCount > 0) && (imageCount > capabilities.maxImageCount))
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = internal_state->surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = internal_state->swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!internal_state->desc.vsync)
        {
            for (auto& present_mode : presentModes)
            {
                if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
                    createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }

        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = internal_state->swapchain;

        VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &internal_state->swapchain));

        if (createInfo.oldSwapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, createInfo.oldSwapchain, nullptr);

        vkGetSwapchainImagesKHR(device, internal_state->swapchain, &imageCount, nullptr);
        internal_state->swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, internal_state->swapchain, &imageCount, internal_state->swapchainImages.data());
        internal_state->swapchainImageFormat = surfaceFormat.format;

        // Create swap chain render targets:
        internal_state->swapchainImageViews.resize(internal_state->swapchainImages.size());
        for (size_t i = 0; i < internal_state->swapchainImages.size(); ++i)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = internal_state->swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = internal_state->swapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (internal_state->swapchainImageViews[i] != VK_NULL_HANDLE)
            {
                allocationHandler->destroylocker.Lock();
                allocationHandler->destroyer_imageviews.push_back(std::make_pair(internal_state->swapchainImageViews[i], allocationHandler->framecount));
                allocationHandler->destroylocker.Unlock();
            }

            VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &internal_state->swapchainImageViews[i]));
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (internal_state->swapchainAcquireSemaphores.empty())
        {
            for (size_t i = 0; i < internal_state->swapchainImages.size(); ++i)
                VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &internal_state->swapchainAcquireSemaphores.emplace_back()));
        }

        if (internal_state->swapchainReleaseSemaphore == VK_NULL_HANDLE)
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &internal_state->swapchainReleaseSemaphore));

        return true;
    }
}

using namespace cyb::rhi::vulkan_internal;

namespace cyb::rhi
{
    void GraphicsDevice_Vulkan::CopyAllocator::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;
    }

    void GraphicsDevice_Vulkan::CopyAllocator::Destroy()
    {
        vkQueueWaitIdle(device->queues[Numerical(QueueType::Copy)].queue);
        for (auto& x : freelist)
        {
            vkDestroyCommandPool(device->device, x.transferCommandPool, nullptr);
            vkDestroyCommandPool(device->device, x.transitionCommandPool, nullptr);
            vkDestroyFence(device->device, x.fence, nullptr);
        }
    }

    GraphicsDevice_Vulkan::CopyAllocator::CopyCMD GraphicsDevice_Vulkan::CopyAllocator::Allocate(uint64_t staging_size)
    {
        CopyCMD cmd;

        locker.Lock();
        // Try to search for a staging buffer that can fit the request:
        for (size_t i = 0; i < freelist.size(); ++i)
        {
            if (freelist[i].uploadBuffer.desc.size >= staging_size)
            {
                if (vkGetFenceStatus(device->device, freelist[i].fence) == VK_SUCCESS)
                {
                    cmd = std::move(freelist[i]);
                    std::swap(freelist[i], freelist.back());
                    freelist.pop_back();
                    break;
                }
            }
        }
        locker.Unlock();

        // If no buffer was found that fits the data, create one:
        if (!cmd.IsValid())
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = device->copyFamily;
            VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &cmd.transferCommandPool));
            poolInfo.queueFamilyIndex = device->graphicsFamily;
            VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &cmd.transitionCommandPool));

            VkCommandBufferAllocateInfo commandBufferInfo = {};
            commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferInfo.commandBufferCount = 1;
            commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferInfo.commandPool = cmd.transferCommandPool;
            VK_CHECK(vkAllocateCommandBuffers(device->device, &commandBufferInfo, &cmd.transferCommandBuffer));
            commandBufferInfo.commandPool = cmd.transitionCommandPool;
            VK_CHECK(vkAllocateCommandBuffers(device->device, &commandBufferInfo, &cmd.transitionCommandBuffer));

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &cmd.fence));
            device->SetFenceName(cmd.fence, "CopyAllocator::fence");

            GPUBufferDesc uploaddesc;
            uploaddesc.size = Max(NextPowerOfTwo(staging_size), uint64_t(65536));
            uploaddesc.cpuAccess = CpuAccessMode::Read;
            bool uploadSuccess = device->CreateBuffer(&uploaddesc, nullptr, &cmd.uploadBuffer);
            assert(uploadSuccess);
            device->SetName(&cmd.uploadBuffer, "CopyAllocator::uploadBuffer");
        }

        // begin command list in valid state:
        VK_CHECK(vkResetCommandPool(device->device, cmd.transferCommandPool, 0));
        VK_CHECK(vkResetCommandPool(device->device, cmd.transitionCommandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(cmd.transferCommandBuffer, &beginInfo));
        VK_CHECK(vkBeginCommandBuffer(cmd.transitionCommandBuffer, &beginInfo));

        VK_CHECK(vkResetFences(device->device, 1, &cmd.fence));

        return cmd;
    }

    void GraphicsDevice_Vulkan::CopyAllocator::Submit(CopyCMD cmd)
    {
        VK_CHECK(vkEndCommandBuffer(cmd.transferCommandBuffer));
        VK_CHECK(vkEndCommandBuffer(cmd.transitionCommandBuffer));

        VkCommandBufferSubmitInfo cmdSubmitInfo = {};
        cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

        VkSemaphoreSubmitInfo copyQueueSignalInfo = {};
        copyQueueSignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

        ScopedMutex lock(locker);

        {
            auto& queue = device->GetQueue(QueueType::Copy);
            cmdSubmitInfo.commandBuffer = cmd.transferCommandBuffer;
            queue.submit_cmds.push_back(cmdSubmitInfo);
            
            queue.Submit(device, VK_NULL_HANDLE);

            copyQueueSignalInfo.semaphore = queue.trackingSemaphore;
            copyQueueSignalInfo.value = queue.lastSubmittedID;
        }

        {
            auto& queue = device->GetQueue(QueueType::Graphics);
            cmdSubmitInfo.commandBuffer = cmd.transitionCommandBuffer;
            queue.submit_waitSemaphoreInfos.push_back(copyQueueSignalInfo);
            queue.submit_cmds.push_back(cmdSubmitInfo);
            
            queue.Submit(device, cmd.fence);    // signal fence on last submit
        }

        while (VK_CHECK(vkWaitForFences(device->device, 1, &cmd.fence, VK_TRUE, timeoutValue)) == VK_TIMEOUT)
        {
            CYB_ERROR("[CopyAllocator::submit] vkWaitForFences resulted in VK_TIMEOUT");
            std::this_thread::yield();
        }

        freelist.push_back(cmd);
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;

        descriptorWrites.reserve(128);
        bufferInfos.reserve(128);
        imageInfos.reserve(128);
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Reset()
    {
        table = {};
        dirtyFlags = true;
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Flush(CommandList cmd)
    {
        if (dirtyFlags == DIRTY_NONE)
            return;

        CommandList_Vulkan& commandlist = device->GetCommandList(cmd);
        auto pso_internal = ToInternal(commandlist.active_pso);
        if (pso_internal->layout_bindings.empty())
            return;

        VkCommandBuffer commandbuffer = commandlist.GetCommandBuffer();

        VkPipelineLayout pipeline_layout = pso_internal->pipelineLayout;
        VkDescriptorSetLayout descriptorset_layout = pso_internal->descriptorset_layout;
        VkDescriptorSet descriptorset = descriptorsetGraphics;
        uint32_t uniform_buffer_dynamic_count = (uint32_t)pso_internal->uniform_buffer_dynamic_slots.size();
        for (size_t i = 0; i < pso_internal->uniform_buffer_dynamic_slots.size(); ++i)
            uniformBufferDynamicOffsets[i] = (uint32_t)table.CBV_offset[pso_internal->uniform_buffer_dynamic_slots[i]];

        if (dirtyFlags & DIRTY_DESCRIPTOR)
        {
            auto& binder_pool = commandlist.binder_pools[device->GetBufferIndex()];

            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = binder_pool.descriptorPool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &descriptorset_layout;

            VkResult res = vkAllocateDescriptorSets(device->device, &alloc_info, &descriptorset);
            while (res == VK_ERROR_OUT_OF_POOL_MEMORY)
            {
                binder_pool.poolSize *= 2;
                binder_pool.Destroy();
                binder_pool.Init(device);
                alloc_info.descriptorPool = binder_pool.descriptorPool;
                res = vkAllocateDescriptorSets(device->device, &alloc_info, &descriptorset);
            }
            assert(res == VK_SUCCESS);

            descriptorWrites.clear();
            bufferInfos.clear();
            imageInfos.clear();

            const auto& layout_bindings = pso_internal->layout_bindings;
            const auto& imageViewTypes = pso_internal->imageViewTypes;

            int i = 0;
            for (const auto& x : layout_bindings)
            {
                if (x.pImmutableSamplers != nullptr)
                {
                    i++;
                    continue;
                }
                VkImageViewType viewType = imageViewTypes[i++];

                for (uint32_t descriptor_index = 0; descriptor_index < x.descriptorCount; ++descriptor_index)
                {
                    uint32_t unrolled_binding = x.binding + descriptor_index;

                    auto& write = descriptorWrites.emplace_back(VkWriteDescriptorSet{});
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = descriptorset;
                    write.dstArrayElement = descriptor_index;
                    write.descriptorType = x.descriptorType;
                    write.dstBinding = x.binding;
                    write.descriptorCount = 1;

                    switch (write.descriptorType)
                    {
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        write.pImageInfo = &imageInfos.emplace_back(VkDescriptorImageInfo{});
                        const GPUResource& resource = table.SRV[unrolled_binding];
                        const Sampler& sampler = table.SAM[unrolled_binding];

                        imageInfos.back().sampler = ToInternal((const Sampler*)&sampler)->resource;
                        auto texture_internal = ToInternal((const Texture*)&resource);
                        imageInfos.back().imageView = texture_internal->srv.imageView;
                        imageInfos.back().imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                    } break;

                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                        imageInfos.emplace_back();
                        write.pImageInfo = &imageInfos.back();
                        imageInfos.back() = {};
                        imageInfos.back().imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                        const GPUResource& resource = table.SRV[unrolled_binding];
                        auto texture_internal = ToInternal((const Texture*)&resource);
                        imageInfos.back().imageView = texture_internal->srv.imageView;
                        imageInfos.back().imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                    } break;

                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                        write.pBufferInfo = &bufferInfos.emplace_back(VkDescriptorBufferInfo{});
                        const uint32_t binding_location = unrolled_binding;
                        const GPUBuffer& buffer = table.CBV[binding_location];
                        assert(buffer.IsBuffer() && "No buffer bound to slot");
                        uint64_t offset = table.CBV_offset[binding_location];

                        auto internal_state = ToInternal(&buffer);
                        bufferInfos.back().buffer = internal_state->resource;
                        bufferInfos.back().offset = offset;
                        bufferInfos.back().range = pso_internal->uniform_buffer_sizes[binding_location];
                        if (bufferInfos.back().range == 0ull)
                            bufferInfos.back().range = VK_WHOLE_SIZE;
                    } break;

                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
                        write.pBufferInfo = &bufferInfos.emplace_back(VkDescriptorBufferInfo{});
                        const uint32_t binding_location = unrolled_binding;
                        const GPUBuffer& buffer = table.CBV[binding_location];
                        assert(buffer.IsBuffer());

                        auto internal_state = ToInternal(&buffer);
                        bufferInfos.back().buffer = internal_state->resource;
                        bufferInfos.back().range = pso_internal->uniform_buffer_sizes[binding_location];
                        if (bufferInfos.back().range == 0ull)
                            bufferInfos.back().range = VK_WHOLE_SIZE;
                    } break;

                    default: assert(0);
                    }
                }
            }

            vkUpdateDescriptorSets(
                device->device,
                (uint32_t)descriptorWrites.size(),
                descriptorWrites.data(),
                0,
                nullptr);
        }

        vkCmdBindDescriptorSets(
            commandbuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout,
            0,
            1,
            &descriptorset,
            uniform_buffer_dynamic_count,
            uniformBufferDynamicOffsets.data());

        descriptorsetGraphics = descriptorset;
        dirtyFlags = DIRTY_NONE;
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;

        // Create descriptor pool:
        VkDescriptorPoolSize poolSizes[5] = {};

        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = DESCRIPTORBINDER_SRV_COUNT * poolSize;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = DESCRIPTORBINDER_SRV_COUNT * poolSize;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = DESCRIPTORBINDER_SRV_COUNT * poolSize;

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[3].descriptorCount = DESCRIPTORBINDER_CBV_COUNT * poolSize;

        poolSizes[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        poolSizes[4].descriptorCount = DESCRIPTORBINDER_CBV_COUNT * poolSize;

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.poolSizeCount = _countof(poolSizes);
        createInfo.pPoolSizes = poolSizes;
        createInfo.maxSets = poolSize;
        VK_CHECK(vkCreateDescriptorPool(device->device, &createInfo, nullptr, &descriptorPool));
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Destroy()
    {
        if (descriptorPool == VK_NULL_HANDLE)
            return;
     
        device->m_allocationHandler->destroylocker.Lock();
        device->m_allocationHandler->destroyer_descriptorPools.push_back(std::make_pair(descriptorPool, device->frameCount));
        descriptorPool = VK_NULL_HANDLE;
        device->m_allocationHandler->destroylocker.Unlock();
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Reset()
    {
        if (descriptorPool == VK_NULL_HANDLE)
            return;

        VK_CHECK(vkResetDescriptorPool(device->device, descriptorPool, 0));
    }

    void GraphicsDevice_Vulkan::ValidatePSO(CommandList cmds)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmds);
        if (!commandlist.dirty_pso)
            return;

        const PipelineState* pso = commandlist.active_pso;
        size_t pipeline_hash = commandlist.prevPipelineHash;
        HashCombine(pipeline_hash, commandlist.vertexbuffer_hash);
        PipelineState_Vulkan* pipelineStateInternal = ToInternal(pso);

        VkPipeline pipeline = VK_NULL_HANDLE;
        auto it = m_pipelinesGlobal.find(pipeline_hash);
        if (it == m_pipelinesGlobal.end())
        {
            for (auto& x : commandlist.pipelinesWorker)
            {
                if (pipeline_hash == x.first)
                {
                    pipeline = x.second;
                    break;
                }
            }

            if (pipeline == VK_NULL_HANDLE)
            {
                // Multisample:
                VkPipelineMultisampleStateCreateInfo multisampling = {};
                multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                multisampling.sampleShadingEnable = VK_FALSE;
                multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

                // Color blending:
                VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
                colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                colorBlendAttachment.blendEnable = VK_TRUE;
                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
                colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

                VkPipelineColorBlendStateCreateInfo colorBlending = {};
                colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                colorBlending.logicOpEnable = VK_FALSE;
                colorBlending.logicOp = VK_LOGIC_OP_COPY;
                colorBlending.attachmentCount = 1;
                colorBlending.pAttachments = &colorBlendAttachment;
                colorBlending.blendConstants[0] = 0.0f;
                colorBlending.blendConstants[1] = 0.0f;
                colorBlending.blendConstants[2] = 0.0f;
                colorBlending.blendConstants[3] = 0.0f;

                // Vertex layout:
                VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
                vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

                std::vector<VkVertexInputBindingDescription> bindings;
                std::vector<VkVertexInputAttributeDescription> attributes;
                if (pso->desc.il != nullptr)
                {
                    uint32_t binding_prev = 0xFFFFFFFF;
                    for (auto& x : pso->desc.il->elements)
                    {
                        if (x.inputSlot == binding_prev)
                            continue;

                        binding_prev = x.inputSlot;
                        VkVertexInputBindingDescription& bind = bindings.emplace_back();
                        bind.binding = x.inputSlot;
                        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                        bind.stride = commandlist.vertexbuffer_strides[x.inputSlot];
                    }

                    uint32_t offset = 0;
                    uint32_t i = 0;
                    binding_prev = 0xFFFFFFFF;
                    for (auto& x : pso->desc.il->elements)
                    {
                        VkVertexInputAttributeDescription attr = {};
                        attr.binding = x.inputSlot;
                        if (attr.binding != binding_prev)
                        {
                            binding_prev = attr.binding;
                            offset = 0;
                        }
                        attr.format = ConvertFormat(x.format);
                        attr.location = i;

                        attr.offset = x.alignedByteOffset;
                        if (attr.offset == VertexInputLayout::APPEND_ALIGNMENT_ELEMENT)
                        {
                            // need to manually resolve this from the format spec.
                            const FormatInfo& formatInfo = GetFormatInfo(x.format);
                            attr.offset = offset;
                            offset += formatInfo.bytesPerBlock;
                        }

                        attributes.push_back(attr);
                        i++;
                    }

                    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
                    vertexInputInfo.pVertexBindingDescriptions = bindings.data();
                    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
                    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();
                }

                // Create pipeline state
                VkGraphicsPipelineCreateInfo pipelineInfo = pipelineStateInternal->pipelineInfo; // make a copy here
                pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pipelineInfo.renderPass = VK_NULL_HANDLE;       // we use VkPipelineRenderingCreateInfo instead
                pipelineInfo.subpass = 0;
                pipelineInfo.pMultisampleState = &multisampling;
                pipelineInfo.pColorBlendState = &colorBlending;
                pipelineInfo.pVertexInputState = &vertexInputInfo;

                VkPipelineRenderingCreateInfo renderingInfo = {};
                renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
                renderingInfo.viewMask = 0;
                renderingInfo.colorAttachmentCount = commandlist.renderpassInfo.rtCount;
                VkFormat formats[8] = {};
                for (uint32_t i = 0; i < commandlist.renderpassInfo.rtCount; ++i)
                {
                    formats[i] = ConvertFormat(commandlist.renderpassInfo.rtFormats[i]);
                }
                renderingInfo.pColorAttachmentFormats = formats;
                renderingInfo.depthAttachmentFormat = ConvertFormat(commandlist.renderpassInfo.dsFormat);
                const FormatInfo& dsFormatInfo = GetFormatInfo(commandlist.renderpassInfo.dsFormat);
                if (dsFormatInfo.hasStencil)
                    renderingInfo.stencilAttachmentFormat = renderingInfo.depthAttachmentFormat;
                pipelineInfo.pNext = &renderingInfo;

                VK_CHECK(vkCreateGraphicsPipelines(device, m_pipelineCache, 1, &pipelineInfo, nullptr, &pipeline));

                commandlist.pipelinesWorker.push_back(std::make_pair(pipeline_hash, pipeline));
            }
        }
        else
        {
            pipeline = it->second;
        }

        assert(pipeline != VK_NULL_HANDLE);
        vkCmdBindPipeline(commandlist.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        commandlist.dirty_pso = false;
    }

    void GraphicsDevice_Vulkan::PreDraw(CommandList cmd)
    {
        ValidatePSO(cmd);
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.binder.Flush(cmd);
    }

    [[nodiscard]] std::vector<const char*> StringSetToVector(const std::unordered_set<std::string>& set)
    {
        std::vector<const char*> vec;
        vec.reserve(set.size());
        for (const auto& s : set)
            vec.push_back(s.c_str());
        return vec;
    }

    GraphicsDevice_Vulkan::GraphicsDevice_Vulkan()
    {
        VK_CHECK(volkInitialize());

        // Fill out application info
        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "CybEngine Application";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "CybEngine";
        applicationInfo.apiVersion = VK_API_VERSION_1_3;

        // Enumerate available layers and extensions:
        uint32_t instanceLayerCount;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
        std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data()));

        uint32_t instanceExtensionCount = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
        std::vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, availableInstanceExtensions.data()));

        constexpr auto hasExtension = [](std::string_view required, const std::vector<VkExtensionProperties>& available) {
            return std::ranges::any_of(available, [&](const VkExtensionProperties& avail) {
                return required == avail.extensionName;
            });
        };

        std::unordered_set<std::string> instanceLayers;
        std::unordered_set<std::string> instanceExtensions;

        // Validate and enable required layer extensions
        const std::unordered_set<std::string> requiredInstanceExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME
        };

        for (auto& required : requiredInstanceExtensions)
        {
            if (!hasExtension(required, availableInstanceExtensions))
                FatalError(std::format("No support for required instance extension {}", required));
            instanceExtensions.insert(required);
        }

        // Validate and enable optional layer extensions
        const std::unordered_map<std::string, bool*> optionalInstanceExtensionMap = {
            { VK_EXT_DEBUG_UTILS_EXTENSION_NAME, &extensions.EXT_debug_utils },
            { VK_EXT_DEBUG_REPORT_EXTENSION_NAME, &extensions.EXT_debug_report }
        };

        for (auto& optional : optionalInstanceExtensionMap)
        {
            if (hasExtension(optional.first, availableInstanceExtensions))
            {
                *(optional.second) = true;
                instanceExtensions.insert(optional.first);
            }
        }

        constexpr auto validateLayers = [](const std::vector<std::string>& required, const std::vector<VkLayerProperties>& available) {
            return std::ranges::all_of(required, [&](std::string_view layer) {
                return std::ranges::any_of(available, [&](const VkLayerProperties& avail) {
                    return layer == avail.layerName;
                });
            });
        };
        
        if (VALIDATION_MODE_ENABLED)
        {
            // Determine the optimal validation layers to enable that are necessary for useful debugging
            const std::vector<std::string> validationLayerPriorityList[] = {
                // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
                { "VK_LAYER_KHRONOS_validation" },

                // Otherwise we fallback to using the LunarG meta layer
                { "VK_LAYER_LUNARG_standard_validation" },

                // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
                {
                    "VK_LAYER_GOOGLE_threading",
                    "VK_LAYER_LUNARG_parameter_validation",
                    "VK_LAYER_LUNARG_object_tracker",
                    "VK_LAYER_LUNARG_core_validation",
                    "VK_LAYER_GOOGLE_unique_objects",
                },

                // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
                { "VK_LAYER_LUNARG_core_validation" }
            };

            for (auto& validationLayers : validationLayerPriorityList)
            {
                if (validateLayers(validationLayers, availableInstanceLayers))
                {
                    for (auto& x : validationLayers)
                        instanceLayers.insert(x);
                    break;  // only need one validation layer
                }
            }
        }

        // Create instance
        const auto instanceLayersVec = StringSetToVector(instanceLayers);
        const auto instanceExtensionsVec = StringSetToVector(instanceExtensions);

        VkInstanceCreateInfo instanceInfo = {};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &applicationInfo;
        instanceInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayersVec.size());
        instanceInfo.ppEnabledLayerNames = instanceLayersVec.data();
        instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensionsVec.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensionsVec.data();

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

        if (VALIDATION_MODE_ENABLED && extensions.EXT_debug_utils)
        {
            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
            instanceInfo.pNext = &debugUtilsCreateInfo;
            CYB_WARNING("Vulkan is running with validation layers enabled. This will heavily impact performace.");
        }

        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));
        volkLoadInstanceOnly(instance);

        if (VALIDATION_MODE_ENABLED && extensions.EXT_debug_utils)
            vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &debugUtilsMessenger);

        // Enumerate and create device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
            FatalError("Failed to find GPU with Vulkan support");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        const std::unordered_set<std::string> requiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
        };
        std::unordered_set<std::string> enabledDeviceExtensions;

        for (const auto& dev : devices)
        {
            bool suitable = true;

            uint32_t deviceExtensionCount;
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &deviceExtensionCount, nullptr);
            std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

            for (auto& requiredExtension : requiredDeviceExtensions)
            {
                if (!hasExtension(requiredExtension, availableDeviceExtensions))
                    suitable = false;
            }
            if (!suitable)
                continue;

            enabledDeviceExtensions = requiredDeviceExtensions;

            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            features2.pNext = &features_1_1;
            features_1_1.pNext = &features_1_2;
            features_1_2.pNext = &features_1_3;
            vkGetPhysicalDeviceFeatures2(dev, &features2);

            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            properties_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
            properties_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
            properties2.pNext = &properties_1_1;
            properties_1_1.pNext = &properties_1_2;
            properties_1_2.pNext = &properties_1_3;
            vkGetPhysicalDeviceProperties2(dev, &properties2);

            bool discreteGPU = properties2.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (discreteGPU || physicalDevice == VK_NULL_HANDLE)
            {
                physicalDevice = dev;

                // if this is discrete GPU, look no further (prioritize discrete GPU)
                if (discreteGPU)
                    break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
            FatalError("Failed to detect a suitable GPU!");

        // Validate and enable optional device extensions
        const std::unordered_map<std::string, bool*> optionalDeviceExtensionMap =  {
            { VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME, &extensions.EXT_depth_clip_enable },
            { VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, &extensions.EXT_conservative_rasterization },
            { VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, &extensions.KHR_fragment_shading_rate },
            { VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME, &extensions.EXT_conditional_rendering },
            { VK_KHR_VIDEO_QUEUE_EXTENSION_NAME, &extensions.KHR_video_queue },
            { VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME, &extensions.KHR_video_decode_queue },
            { VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME, &extensions.KHR_video_decode_h264 }
        };

        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

        for (auto& optional : optionalDeviceExtensionMap)
        {
            if (hasExtension(optional.first, availableDeviceExtensions))
            {
                *(optional.second) = true;
                enabledDeviceExtensions.insert(optional.first);
            }
        }

        // Find queue families:
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        queueFamilies.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        // Query base queue families:
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (graphicsFamily == VK_QUEUE_FAMILY_IGNORED &&
                queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily = i;
            if (copyFamily == VK_QUEUE_FAMILY_IGNORED &&
                queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
                copyFamily = i;
            if (computeFamily == VK_QUEUE_FAMILY_IGNORED &&
                queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                computeFamily = i;
        }

        // Now try to find dedicated compute and transfer queues:
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueCount > 0 &&
                queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                copyFamily = i;
            }

            if (queueFamilies[i].queueCount > 0 &&
                queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                computeFamily = i;
            }
        }

        std::unordered_set<uint32_t> uniqueQueueFamilies = { graphicsFamily, copyFamily, computeFamily };

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
            families.push_back(queueFamily);
        }

        const auto deviceExtensionsVec = StringSetToVector(enabledDeviceExtensions);

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        device_info.pQueueCreateInfos = queueCreateInfos.data();
        device_info.pEnabledFeatures = nullptr;
        device_info.pNext = &features2;
        device_info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionsVec.size());
        device_info.ppEnabledExtensionNames = deviceExtensionsVec.data();

        VK_CHECK(vkCreateDevice(physicalDevice, &device_info, nullptr, &device));
        volkLoadDevice(device);    

        // Queues:
        {
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            VkQueue computeQueue = VK_NULL_HANDLE;
            VkQueue copyQueue = VK_NULL_HANDLE;
            vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
            vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
            vkGetDeviceQueue(device, copyFamily, 0, &copyQueue);

            queues[Numerical(QueueType::Graphics)].queue = graphicsQueue;
            queues[Numerical(QueueType::Graphics)].locker = std::make_shared<Mutex>();
            queues[Numerical(QueueType::Compute)].queue = computeQueue;
            queues[Numerical(QueueType::Compute)].locker = std::make_shared<Mutex>();
            queues[Numerical(QueueType::Copy)].queue = copyQueue;
            queues[Numerical(QueueType::Copy)].locker = std::make_shared<Mutex>();
        }

        memory_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memory_properties_2);

        m_allocationHandler = std::make_shared<AllocationHandler>();
        m_allocationHandler->device = device;
        m_allocationHandler->instance = instance;

        // initialize vulkan memory allocator helper:
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;

        if (features_1_2.bufferDeviceAddress)
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

#if VMA_DYNAMIC_VULKAN_FUNCTIONS
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;
#endif
        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocationHandler->allocator));

        m_copyAllocator.Init(this);

        // Create frame resources:
        {
            // create a timeline semaphore in each queue for state tracking
            for (uint32_t i = 0; i < Numerical(QueueType::Count); ++i)
            {
                VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
                timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                timelineCreateInfo.initialValue = 0;

                VkSemaphoreCreateInfo semaphoreInfo = {};
                semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semaphoreInfo.pNext = &timelineCreateInfo;

                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &queues[i].trackingSemaphore);
                switch (static_cast<QueueType>(i))
                {
                case QueueType::Graphics:
                    SetSemaphoreName(queues[i].trackingSemaphore, "CommandQueue::trackingSemaphore[QueueType::Graphics]");
                    break;
                case QueueType::Compute:
                    SetSemaphoreName(queues[i].trackingSemaphore, "CommandQueue::trackingSemaphore[QueueType::Compute]");
                    break;
                case QueueType::Copy:
                    SetSemaphoreName(queues[i].trackingSemaphore, "CommandQueue::trackingSemaphore[QueueType::Copy]");
                    break;
                }
            }
        }

        gpuTimestampFrequency = uint64_t(1.0 / double(properties2.properties.limits.timestampPeriod) * 1000 * 1000 * 1000);

        // Dynamic PSO states:
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount = (uint32_t)pso_dynamic_states.size();
        dynamic_state_info.pDynamicStates = pso_dynamic_states.data();

        // Create pipeline cache
        // TODO: Load pipeline cache from disk
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));

        CYB_INFO("Initialized Vulkan {}.{}", VK_API_VERSION_MAJOR(properties2.properties.apiVersion), VK_API_VERSION_MINOR(properties2.properties.apiVersion));
        CYB_INFO("  Device: {}", properties2.properties.deviceName);
        CYB_INFO("  Driver: {} {}", properties_1_2.driverName, properties_1_2.driverInfo);
        CYB_TRACE("VulkanDeviceExtension: {}", deviceExtensionsVec);
        CYB_TRACE("VulkanInstanceExtensions: {}", instanceExtensionsVec);
        CYB_TRACE("VulkanInstanceLayers: {}", instanceLayersVec);
    }

    GraphicsDevice_Vulkan::~GraphicsDevice_Vulkan()
    {
        VK_CHECK(vkDeviceWaitIdle(device));

        for (auto& x : m_pipelinesGlobal)
            vkDestroyPipeline(device, x.second, nullptr);

        if (debugUtilsMessenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);

        m_copyAllocator.Destroy();

        for (auto& x : m_psoLayoutCache)
        {
            vkDestroyPipelineLayout(device, x.second.pipeline_layout, nullptr);
            vkDestroyDescriptorSetLayout(device, x.second.descriptorset_layout, nullptr);
        }

        if (m_pipelineCache != VK_NULL_HANDLE)
        {
            // TODO: Save pipeline cache to disk
            vkDestroyPipelineCache(device, m_pipelineCache, nullptr);
            m_pipelineCache = VK_NULL_HANDLE;
        }

        for (auto& commandlist : m_commandlists)
        {
            for (uint32_t buffer_index = 0; buffer_index < BUFFERCOUNT; ++buffer_index)
            {
                for (uint32_t q = 0; q < static_cast<uint32_t>(QueueType::Count); ++q)
                {
                    vkDestroyCommandPool(device, commandlist->commandpools[buffer_index][q], nullptr);
                }
            }

            for (auto& x : commandlist->binder_pools)
                x.Destroy();
        }

        for (auto& queue : queues)
            vkDestroySemaphore(device, queue.trackingSemaphore, nullptr);
    }

    bool GraphicsDevice_Vulkan::CreateSwapchain(const SwapchainDesc* desc, WindowHandle window, Swapchain* swapchain) const
    {
        auto internal_state = std::static_pointer_cast<Swapchain_Vulkan>(swapchain->internal_state);
        if (swapchain->internal_state == nullptr)
            internal_state = std::make_shared<Swapchain_Vulkan>();

        internal_state->allocationhandler = m_allocationHandler;
        internal_state->desc = *desc;
        swapchain->internal_state = internal_state;
        swapchain->desc = *desc;

        // Surface creation:
        if (internal_state->surface == VK_NULL_HANDLE)
        {
#ifdef _WIN32
            VkWin32SurfaceCreateInfoKHR create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            create_info.hwnd = window;
            create_info.hinstance = GetModuleHandle(nullptr);
            vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &internal_state->surface);
#else
#error VULKAN DEVICE ERROR: PLATFORM NOT SUPPORTED
#endif // _WIN32
        }

        uint32_t presentFamily = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); ++i)
        {
            VkBool32 graphicsSupport = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT);
            VkBool32 presentationSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, internal_state->surface, &presentationSupport);

            if (graphicsSupport && presentationSupport)
            {
                presentFamily = i;
                break;
            }
        }

        // present family not found, we cannot create Swapchain
        if (presentFamily == VK_QUEUE_FAMILY_IGNORED)
            return false;

        return CreateSwapchainInternal(internal_state.get(), physicalDevice, device, m_allocationHandler);
    }

    bool GraphicsDevice_Vulkan::CreateBuffer(const GPUBufferDesc* desc, const void* initData, GPUBuffer* buffer) const
    {
        auto internal_state = std::make_shared<Buffer_Vulkan>();
        internal_state->allocationhandler = m_allocationHandler;
        buffer->internal_state = internal_state;
        buffer->type = GPUResource::Type::Buffer;
        buffer->mappedData = nullptr;
        buffer->mappedSize = 0;
        buffer->desc = *desc;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = buffer->desc.size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (HasFlag(buffer->desc.usage, BufferUsage::VertexBufferBit))
            bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (HasFlag(buffer->desc.usage, BufferUsage::IndexBufferBit))
            bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (HasFlag(buffer->desc.usage, BufferUsage::ConstantBufferBit))
            bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        bufferInfo.flags = 0;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = (desc->cpuAccess != CpuAccessMode::None) ?
            VMA_MEMORY_USAGE_AUTO_PREFER_HOST :
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (desc->cpuAccess == CpuAccessMode::Write)
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        else if (desc->cpuAccess == CpuAccessMode::Read)
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_CHECK(vmaCreateBuffer(
            m_allocationHandler->allocator,
            &bufferInfo,
            &allocInfo,
            &internal_state->resource,
            &internal_state->allocation,
            nullptr
        ));

        if (desc->cpuAccess != CpuAccessMode::None)
        {
            VmaAllocationInfo vmaInfo{};
            vmaGetAllocationInfo(m_allocationHandler->allocator, internal_state->allocation, &vmaInfo);
            buffer->mappedData = vmaInfo.pMappedData;
            buffer->mappedSize = vmaInfo.size;
        }

        // Issue data copy on request:
        if (initData != nullptr)
        {
            auto cmd = m_copyAllocator.Allocate(desc->size);
            std::memcpy(cmd.uploadBuffer.mappedData, initData, buffer->desc.size);

            VkBufferCopy copyRegion = {};
            copyRegion.size = buffer->desc.size;
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;

            vkCmdCopyBuffer(
                cmd.transferCommandBuffer,
                ToInternal(&cmd.uploadBuffer)->resource,
                internal_state->resource,
                1,
                &copyRegion
            );

            m_copyAllocator.Submit(cmd);
        }

        return true;
    }

    bool GraphicsDevice_Vulkan::CreateQuery(const GPUQueryDesc* desc, GPUQuery* query) const
    {
        auto internal_state = std::make_shared<Query_Vulkan>();
        internal_state->allocationhandler = m_allocationHandler;
        query->internal_state = internal_state;
        query->desc = *desc;

        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryCount = desc->queryCount;

        switch (desc->type)
        {
        case GPUQueryType::Timestamp:
            poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            break;
        case GPUQueryType::Occlusion:
        case GPUQueryType::OcclusionBinary:
            poolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
            break;
        }

        VK_CHECK(vkCreateQueryPool(device, &poolInfo, nullptr, &internal_state->pool));
        return true;
    }

    void GraphicsDevice_Vulkan::BindVertexBuffers(const GPUBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd)
    {
        assert(count <= 8);
        CommandList_Vulkan& commandList = GetCommandList(cmd);
        size_t hash = 0;

        VkDeviceSize voffsets[8] = {};
        VkBuffer vbuffers[8] = {};

        for (uint32_t i = 0; i < count; ++i)
        {
            HashCombine(hash, strides[i]);
            commandList.vertexbuffer_strides[i] = strides[i];

            auto internal_state = ToInternal(vertexBuffers[i]);
            vbuffers[i] = internal_state->resource;
            if (offsets != nullptr)
            {
                voffsets[i] = offsets[i];
            }
        }

        for (int i = count; i < _countof(commandList.vertexbuffer_strides); ++i)
        {
            commandList.vertexbuffer_strides[i] = 0;
        }

        vkCmdBindVertexBuffers(commandList.GetCommandBuffer(), 0, static_cast<uint32_t>(count), vbuffers, voffsets);

        if (hash != commandList.vertexbuffer_hash)
        {
            commandList.vertexbuffer_hash = hash;
            commandList.dirty_pso = true;
        }
    }

    void GraphicsDevice_Vulkan::BindIndexBuffer(const GPUBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd)
    {
        if (index_buffer == nullptr)
            return;

        auto internal_state = ToInternal(index_buffer);
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdBindIndexBuffer(commandlist.GetCommandBuffer(), internal_state->resource, offset, format == IndexBufferFormat::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    void GraphicsDevice_Vulkan::BindStencilRef(uint32_t value, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdSetStencilReference(commandlist.GetCommandBuffer(), VK_STENCIL_FRONT_AND_BACK, value);
    }

    void GraphicsDevice_Vulkan::BindResource(const GPUResource* resource, int slot, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        assert(slot < DESCRIPTORBINDER_SRV_COUNT);
        auto& binder = commandlist.binder;
        if (binder.table.SRV[slot].internal_state != resource->internal_state)
        {
            binder.table.SRV[slot] = *resource;
            binder.dirtyFlags |= DescriptorBinder::DIRTY_DESCRIPTOR;
        }
    }

    void GraphicsDevice_Vulkan::BindSampler(const Sampler* sampler, uint32_t slot, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        assert(slot < DESCRIPTORBINDER_SAMPLER_COUNT);
        auto& binder = commandlist.binder;
        if (binder.table.SAM[slot].internal_state != sampler->internal_state)
        {
            binder.table.SAM[slot] = *sampler;
            binder.dirtyFlags |= DescriptorBinder::DIRTY_DESCRIPTOR;
        }
    }

    void GraphicsDevice_Vulkan::BindConstantBuffer(const GPUBuffer* buffer, uint32_t slot, CommandList cmd, uint64_t offset)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        assert(slot < DESCRIPTORBINDER_CBV_COUNT);
        auto& binder = commandlist.binder;

        if (binder.table.CBV[slot].internal_state != buffer->internal_state)
        {
            binder.table.CBV[slot] = *buffer;
            binder.dirtyFlags |= DescriptorBinder::DIRTY_DESCRIPTOR;
        }

        if (binder.table.CBV_offset[slot] != offset)
        {
            binder.table.CBV_offset[slot] = offset;
            binder.dirtyFlags |= DescriptorBinder::DIRTY_OFFSET;
        }
    }

    void GraphicsDevice_Vulkan::CopyBuffer(const GPUBuffer* pDst, uint64_t dst_offset, const GPUBuffer* pSrc, uint64_t src_offset, uint64_t size, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto src_internal = ToInternal((const GPUBuffer*)pSrc);
        auto dst_internal = ToInternal((const GPUBuffer*)pDst);

        VkBufferCopy copy = {};
        copy.srcOffset = src_offset;
        copy.dstOffset = dst_offset;
        copy.size = size;

        vkCmdCopyBuffer(commandlist.GetCommandBuffer(),
            src_internal->resource,
            dst_internal->resource,
            1, &copy
        );
    }

    void GraphicsDevice_Vulkan::CreateSubresource(Texture* texture, SubresourceType type, uint32_t firstSlice, uint32_t sliceCount, uint32_t firstMip, uint32_t mipCount) const
    {
        Texture_Vulkan* textureInternal = ToInternal(texture);
        Format format = texture->GetDesc().format;

        Texture_Vulkan::TextureSubresource subresource;
        subresource.firstMip = firstMip;
        subresource.mipCount = mipCount;
        subresource.firstSlice = firstSlice;
        subresource.sliceCount = sliceCount;

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureInternal->resource;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = ConvertFormat(format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseArrayLayer = firstSlice;
        viewInfo.subresourceRange.layerCount = sliceCount;
        viewInfo.subresourceRange.baseMipLevel = firstMip;
        viewInfo.subresourceRange.levelCount = mipCount;

        switch (type)
        {
        case SubresourceType::SRV: {
            const Swizzle& swizzle = texture->GetDesc().swizzle;
            viewInfo.components.r = ConvertComponentSwizzle(swizzle.r);
            viewInfo.components.g = ConvertComponentSwizzle(swizzle.g);
            viewInfo.components.b = ConvertComponentSwizzle(swizzle.b);
            viewInfo.components.a = ConvertComponentSwizzle(swizzle.a);

            const FormatInfo& formatInfo = GetFormatInfo(format);
            if (formatInfo.hasDepth)
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &subresource.imageView));
            assert(!textureInternal->srv.IsValid());
            textureInternal->srv = subresource;
        } break;
        case SubresourceType::RTV: {
            viewInfo.subresourceRange.levelCount = 1;
            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &subresource.imageView));
            assert(!textureInternal->rtv.IsValid());
            textureInternal->rtv = subresource;
        } break;
        case SubresourceType::DSV: {
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &subresource.imageView));
            assert(!textureInternal->dsv.IsValid());
            textureInternal->dsv = subresource;
        } break;

        default:
            assert(0);
            break;
        }
    }

    static VkImageUsageFlags ConvertImageUsage(ResourceStates states)
    {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (HasFlag(states, ResourceStates::ShaderResourceBit))
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (HasFlag(states, ResourceStates::UnorderedAccessBit))
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        if (HasFlag(states, ResourceStates::RenderTargetBit))
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (HasFlag(states, ResourceStates::DepthWriteBit) ||
            HasFlag(states, ResourceStates::DepthReadBit))
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        return usage;
    }

    static VkAccessFlags ConvertAccessMask(ResourceStates state)
    {
        switch (state)
        {
        case ResourceStates::ShaderResourceBit:
            return VK_ACCESS_SHADER_READ_BIT;

        case ResourceStates::UnorderedAccessBit:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        case ResourceStates::CopySourceBit:
            return VK_ACCESS_TRANSFER_READ_BIT;

        case ResourceStates::CopyDestBit:
            return VK_ACCESS_TRANSFER_WRITE_BIT;

        case ResourceStates::RenderTargetBit:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        case ResourceStates::DepthWriteBit:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        case ResourceStates::DepthReadBit:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        case ResourceStates::VertexBufferBit:
            return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

        case ResourceStates::IndexBufferBit:
            return VK_ACCESS_INDEX_READ_BIT;

        case ResourceStates::ConstantBufferBit:
            return VK_ACCESS_UNIFORM_READ_BIT;

        case ResourceStates::IndirectArgumentBit:
            return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        case ResourceStates::AccelStructBit:
            return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

        default:
            return 0;
        }
    }

    bool GraphicsDevice_Vulkan::CreateTexture(const TextureDesc* desc, const SubresourceData* initData, Texture* texture) const
    {
        assert(texture != nullptr);
        assert(desc->format != Format::Unknown);
        std::shared_ptr<Texture_Vulkan> textureInternal = std::make_shared<Texture_Vulkan>();
        textureInternal->allocationHandler = m_allocationHandler;
        texture->internal_state = textureInternal;
        texture->type = GPUResource::Type::Texture;
        texture->desc = *desc;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.format = ConvertFormat(texture->desc.format);
        imageInfo.extent.width = texture->desc.width;
        imageInfo.extent.height = texture->desc.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.imageType = ConvertTextureType(desc->type);
        imageInfo.usage = ConvertImageUsage(desc->initialState);

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VK_CHECK(vmaCreateImage(
            m_allocationHandler->allocator,
            &imageInfo,
            &allocInfo,
            &textureInternal->resource,
            &textureInternal->allocation,
            nullptr));

        const FormatInfo& formatInfo = GetFormatInfo(desc->format);

        if (initData != nullptr)
        {
            CopyAllocator::CopyCMD cmd = m_copyAllocator.Allocate(textureInternal->allocation->GetSize());

            std::vector<VkBufferImageCopy> copyRegions;

            VkDeviceSize copyOffset = 0;
            uint32_t initDataIndex = 0;
            for (uint32_t layer = 0; layer < desc->arraySize; ++layer)
            {
                uint32_t width = imageInfo.extent.width;
                uint32_t height = imageInfo.extent.height;
                uint32_t depth = imageInfo.extent.depth;
                for (uint32_t mip = 0; mip < desc->mipLevels; ++mip)
                {
                    const SubresourceData& subresourceData = initData[initDataIndex++];
                    assert(subresourceData.mem != nullptr);
                    const uint32_t numBlocksX = width / formatInfo.blockSize;
                    const uint32_t numBlocksY = height / formatInfo.blockSize;
                    const uint32_t dstRowPitch = numBlocksX * formatInfo.bytesPerBlock;
                    const uint32_t dstSlicePitch = dstRowPitch * numBlocksY;
                    const uint32_t srcRowPitch = subresourceData.rowPitch;
                    const uint32_t srcSlicePitch = subresourceData.slicePitch;
                    for (uint32_t z = 0; z < depth; ++z)
                    {
                        uint8_t* dstSlice = (uint8_t*)cmd.uploadBuffer.mappedData + copyOffset + dstSlicePitch * z;
                        uint8_t* srcSlice = (uint8_t*)subresourceData.mem + srcSlicePitch * z;
                        for (uint32_t y = 0; y < numBlocksY; ++y)
                        {
                            std::memcpy(
                                dstSlice + dstRowPitch * y,
                                srcSlice + srcRowPitch * y,
                                dstRowPitch
                            );
                        }
                    }

                    if (cmd.IsValid())
                    {
                        VkBufferImageCopy copyRegion = {};
                        copyRegion.bufferOffset = copyOffset;
                        copyRegion.bufferRowLength = 0;
                        copyRegion.bufferImageHeight = 0;

                        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        copyRegion.imageSubresource.mipLevel = mip;
                        copyRegion.imageSubresource.baseArrayLayer = layer;
                        copyRegion.imageSubresource.layerCount = 1;

                        copyRegion.imageOffset = { 0, 0, 0 };
                        copyRegion.imageExtent = {
                            width,
                            height,
                            depth
                        };

                        copyRegions.push_back(copyRegion);
                    }

                    copyOffset += dstSlicePitch * depth;
                    copyOffset = AlignTo(copyOffset, VkDeviceSize(4));
                    width = std::max(1u, width / 2);
                    height = std::max(1u, height / 2);
                    depth = std::max(1u, depth / 2);
                }
            }

            if (cmd.IsValid())
            {
                VkImageMemoryBarrier2 barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.image = textureInternal->resource;
                barrier.oldLayout = imageInfo.initialLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                barrier.srcAccessMask = 0;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = imageInfo.arrayLayers;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = imageInfo.mipLevels;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                VkDependencyInfo dependencyInfo = {};
                dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.imageMemoryBarrierCount = 1;
                dependencyInfo.pImageMemoryBarriers = &barrier;

                vkCmdPipelineBarrier2(cmd.transferCommandBuffer, &dependencyInfo);

                vkCmdCopyBufferToImage(
                    cmd.transferCommandBuffer,
                    ToInternal(&cmd.uploadBuffer)->resource,
                    textureInternal->resource,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    (uint32_t)copyRegions.size(),
                    copyRegions.data());

                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = ConvertImageLayout(desc->initialState);
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = ConvertAccessMask(desc->initialState);
                std::swap(barrier.srcStageMask, barrier.dstStageMask);
                
                vkCmdPipelineBarrier2(cmd.transitionCommandBuffer, &dependencyInfo);
                m_copyAllocator.Submit(cmd);
            }
        }
        else
        {
            VkImageMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.image = textureInternal->resource;
            barrier.oldLayout = imageInfo.initialLayout;
            barrier.newLayout = ConvertImageLayout(desc->initialState);
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            barrier.dstAccessMask = ConvertAccessMask(desc->initialState);
            if (formatInfo.hasDepth)
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if (formatInfo.hasStencil)
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            else
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = imageInfo.arrayLayers;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = imageInfo.mipLevels;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            CopyAllocator::CopyCMD cmd = m_copyAllocator.Allocate(0);
           
            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &barrier;

            vkCmdPipelineBarrier2(cmd.transitionCommandBuffer, &dependencyInfo);
            m_copyAllocator.Submit(cmd);
        }

        if (HasFlag(desc->initialState, ResourceStates::ShaderResourceBit))
            CreateSubresource(texture, SubresourceType::SRV, 0, 1, 0, 1);
        if (HasFlag(desc->initialState, ResourceStates::RenderTargetBit))
            CreateSubresource(texture, SubresourceType::RTV, 0, 1, 0, 1);
        if (HasFlag(desc->initialState, ResourceStates::DepthReadBit) ||
            HasFlag(desc->initialState, ResourceStates::DepthWriteBit))
            CreateSubresource(texture, SubresourceType::DSV, 0, 1, 0, 1);

        return true;
    }

    GraphicsDevice::MemoryUsage GraphicsDevice_Vulkan::GetMemoryUsage() const
    {
        MemoryUsage result = {};
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
        vmaGetHeapBudgets(m_allocationHandler->allocator, budgets);
        for (uint32_t i = 0; i < memory_properties_2.memoryProperties.memoryHeapCount; ++i)
        {
            if (memory_properties_2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                result.budget += budgets[i].budget;
                result.usage += budgets[i].usage;
            }
        }
        return result;
    }

    uint64_t GraphicsDevice_Vulkan::GetMinOffsetAlignment(const GPUBufferDesc* desc) const
    {
        uint64_t alignment = 1u;
        if (HasFlag(desc->usage, BufferUsage::ConstantBufferBit))
            alignment = std::max(alignment, properties2.properties.limits.minUniformBufferOffsetAlignment);
        else
            alignment = std::max(alignment, properties2.properties.limits.minTexelBufferOffsetAlignment);
        return alignment;
    }

    bool GraphicsDevice_Vulkan::CreateShader(ShaderType stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const
    {
        assert(shader != nullptr);
        assert(shaderBytecode != nullptr);
        assert(bytecodeLength > 0);

        auto internal_state = std::make_shared<Shader_Vulkan>();
        internal_state->allocationhandler = m_allocationHandler;
        shader->internal_state = internal_state;
        shader->stage = stage;

        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = bytecodeLength;
        create_info.pCode = (const uint32_t*)shaderBytecode;
        VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &internal_state->shadermodule));

        internal_state->stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        internal_state->stageInfo.module = internal_state->shadermodule;
        internal_state->stageInfo.pName = "main";
        switch (stage)
        {
        case ShaderType::Vertex:
            internal_state->stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderType::Geometry:
            internal_state->stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ShaderType::Pixel:
            internal_state->stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        default:
            // also means library shader (ray tracing)
            internal_state->stageInfo.stage = VK_SHADER_STAGE_ALL;
            break;
        }

        {
            SpvReflectShaderModule module;
            [[maybe_unused]] SpvReflectResult result = spvReflectCreateShaderModule(create_info.codeSize, create_info.pCode, &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            uint32_t binding_count = 0;
            result = spvReflectEnumerateDescriptorBindings(
                &module, &binding_count, nullptr
            );
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
            result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            uint32_t pushCount = 0;
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectBlockVariable*> pushconstants(pushCount);
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushconstants.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            for (auto& x : pushconstants)
            {
                auto& push = internal_state->pushconstants;
                push.stageFlags = internal_state->stageInfo.stage;
                push.offset = x->offset;
                push.size = x->size;
            }

            for (auto& x : bindings)
            {
                // NO SUPPORT FOR BINDLESS ATM
                assert((x->set > 0) == false);

                auto& descriptor = internal_state->layout_bindings.emplace_back();
                descriptor.stageFlags = internal_state->stageInfo.stage;
                descriptor.binding = x->binding;
                descriptor.descriptorCount = x->count;
                descriptor.descriptorType = (VkDescriptorType)x->descriptor_type;

                auto& imageViewType = internal_state->imageViewTypes.emplace_back();
                imageViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

                if (descriptor.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    // For now, always replace VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER with VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                    //	It would be quite messy to track which buffer is dynamic and which is not in the binding code, consider multiple pipeline bind points too
                    //	But maybe the dynamic uniform buffer is not always best because it occupies more registers (like DX12 root descriptor)?
                    descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    for (uint32_t i = 0; i < descriptor.descriptorCount; ++i)
                    {
                        internal_state->uniform_buffer_sizes[descriptor.binding + i] = x->block.size;
                        internal_state->uniform_buffer_dynamic_slots.push_back(descriptor.binding + i);
                    }
                }

                switch (x->descriptor_type)
                {
                default:
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    switch (x->image.dim)
                    {
                    default:
                    case SpvDim1D:
                        if (x->image.arrayed == 0)
                            imageViewType = VK_IMAGE_VIEW_TYPE_1D;
                        else
                            imageViewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
                        break;
                    case SpvDim2D:
                        if (x->image.arrayed == 0)
                            imageViewType = VK_IMAGE_VIEW_TYPE_2D;
                        else
                            imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                        break;
                    case SpvDim3D:
                        imageViewType = VK_IMAGE_VIEW_TYPE_3D;
                        break;
                    case SpvDimCube:
                        if (x->image.arrayed == 0)
                            imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
                        else
                            imageViewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                        break;
                    }
                    break;
                }
            }

            spvReflectDestroyShaderModule(&module);
        }

        return true;
    }

    bool GraphicsDevice_Vulkan::CreateSampler(const SamplerDesc* desc, Sampler* sampler) const
    {
        auto internal_state = std::make_shared<Sampler_Vulkan>();
        internal_state->allocationhandler = m_allocationHandler;
        sampler->internal_state = internal_state;
        sampler->desc = *desc;

        const bool anisotropyEnable = desc->maxAnisotropy > 1.0f;

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.flags = 0;
        samplerInfo.pNext = nullptr;
        samplerInfo.minFilter = HasFlag(desc->filter, Filtering::Min) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        samplerInfo.magFilter = HasFlag(desc->filter, Filtering::Mag) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = HasFlag(desc->filter, Filtering::Mip) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.anisotropyEnable = anisotropyEnable;
        samplerInfo.maxAnisotropy = anisotropyEnable ? desc->maxAnisotropy : 1.0f;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.addressModeU = ConvertSamplerAddressMode(desc->addressU);
        samplerInfo.addressModeV = ConvertSamplerAddressMode(desc->addressV);
        samplerInfo.addressModeW = ConvertSamplerAddressMode(desc->addressW);
        samplerInfo.mipLodBias = desc->lodBias;
        samplerInfo.minLod = desc->minLOD;
        samplerInfo.maxLod = desc->maxLOD;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &internal_state->resource));
        return true;
    }

    bool GraphicsDevice_Vulkan::CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const
    {
        auto internal_state = std::make_shared<PipelineState_Vulkan>();
        pso->internal_state = internal_state;
        pso->desc = *desc;

        pso->hash = 0;
        HashCombine(pso->hash, desc->vs);
        HashCombine(pso->hash, desc->gs);
        HashCombine(pso->hash, desc->ps);
        HashCombine(pso->hash, desc->rs);
        HashCombine(pso->hash, desc->dss);
        HashCombine(pso->hash, desc->il);
        HashCombine(pso->hash, desc->pt);

        // Create bindings:
        {
            auto insert_shader = [&] (const Shader* shader) {
                if (shader == nullptr)
                    return;
                auto shader_internal = ToInternal(shader);

                uint32_t i = 0;
                for (auto& shader_binding : shader_internal->layout_bindings)
                {
                    bool found = false;
                    for (auto& pipeline_binding : internal_state->layout_bindings)
                    {
                        if (shader_binding.binding == pipeline_binding.binding)
                        {
                            assert(shader_binding.descriptorCount == pipeline_binding.descriptorCount);
                            assert(shader_binding.descriptorType == pipeline_binding.descriptorType);
                            found = true;
                            pipeline_binding.stageFlags |= shader_binding.stageFlags;
                            break;
                        }
                    }

                    if (!found)
                    {
                        internal_state->layout_bindings.push_back(shader_binding);
                        internal_state->imageViewTypes.push_back(shader_internal->imageViewTypes[i]);

                        if (shader_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                            shader_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        {
                            for (uint32_t k = 0; k < shader_binding.descriptorCount; ++k)
                            {
                                internal_state->uniform_buffer_sizes[shader_binding.binding + k] = shader_internal->uniform_buffer_sizes[shader_binding.binding + k];
                            }

                            if (shader_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                            {
                                for (uint32_t k = 0; k < shader_binding.descriptorCount; ++k)
                                {
                                    internal_state->uniform_buffer_dynamic_slots.push_back(shader_binding.binding + k);
                                }
                            }
                        }
                    }

                    i++;
                }

                if (shader_internal->pushconstants.size > 0)
                {
                    internal_state->pushconstants.offset = std::min(internal_state->pushconstants.offset, shader_internal->pushconstants.offset);
                    internal_state->pushconstants.size = std::max(internal_state->pushconstants.size, shader_internal->pushconstants.size);
                    internal_state->pushconstants.stageFlags |= shader_internal->pushconstants.stageFlags;
                }
            };

            insert_shader(desc->vs);
            insert_shader(desc->gs);
            insert_shader(desc->ps);

            // sort because dynamic offsets array is tightly packed to match slot numbers:
            std::sort(internal_state->uniform_buffer_dynamic_slots.begin(), internal_state->uniform_buffer_dynamic_slots.end());
        }

        internal_state->binding_hash = 0;
        uint32_t i = 0;
        for (auto& x : internal_state->layout_bindings)
        {
            HashCombine(internal_state->binding_hash, x.binding);
            HashCombine(internal_state->binding_hash, x.descriptorCount);
            HashCombine(internal_state->binding_hash, x.descriptorType);
            HashCombine(internal_state->binding_hash, x.stageFlags);
            HashCombine(internal_state->binding_hash, internal_state->imageViewTypes[i++]);
        }
        HashCombine(internal_state->binding_hash, internal_state->pushconstants.offset);
        HashCombine(internal_state->binding_hash, internal_state->pushconstants.size);
        HashCombine(internal_state->binding_hash, internal_state->pushconstants.stageFlags);

        m_psoLayoutCacheMutex.Lock();
        if (m_psoLayoutCache[internal_state->binding_hash].pipeline_layout == VK_NULL_HANDLE)
        {
            VkDescriptorSetLayoutCreateInfo descriptorset_layout_info = {};
            descriptorset_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorset_layout_info.bindingCount = static_cast<uint32_t>(internal_state->layout_bindings.size());
            descriptorset_layout_info.pBindings = internal_state->layout_bindings.data();
            VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorset_layout_info, nullptr, &internal_state->descriptorset_layout));

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &internal_state->descriptorset_layout;
            if (internal_state->pushconstants.size > 0)
            {
                pipelineLayoutInfo.pushConstantRangeCount = 1;
                pipelineLayoutInfo.pPushConstantRanges = &internal_state->pushconstants;
            }
            else
            {
                pipelineLayoutInfo.pushConstantRangeCount = 0;
                pipelineLayoutInfo.pPushConstantRanges = nullptr;
            }
            VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &internal_state->pipelineLayout));

            m_psoLayoutCache[internal_state->binding_hash].descriptorset_layout = internal_state->descriptorset_layout;
            m_psoLayoutCache[internal_state->binding_hash].pipeline_layout = internal_state->pipelineLayout;
        }
        else
        {
            internal_state->descriptorset_layout = m_psoLayoutCache[internal_state->binding_hash].descriptorset_layout;
            internal_state->pipelineLayout = m_psoLayoutCache[internal_state->binding_hash].pipeline_layout;
        }
        m_psoLayoutCacheMutex.Unlock();

        // Viewport & Scissors:
        VkViewport& viewport = internal_state->viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 65535;
        viewport.height = 65535;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 0.1f;

        VkPipelineViewportStateCreateInfo& viewportState = internal_state->viewportState;
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 0;
        viewportState.pScissors = nullptr;

        // Depth-Stencil:
        VkPipelineDepthStencilStateCreateInfo& depthstencil = internal_state->depthstencil;
        depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (pso->desc.dss != nullptr)
        {
            depthstencil.depthTestEnable = pso->desc.dss->depthEnable ? VK_TRUE : VK_FALSE;
            depthstencil.depthWriteEnable = pso->desc.dss->depthWriteMask == DepthWriteMask::Zero ? VK_FALSE : VK_TRUE;
            depthstencil.depthCompareOp = ConvertComparisonFunc(pso->desc.dss->depthFunc);

            depthstencil.stencilTestEnable = pso->desc.dss->stencilEnable ? VK_TRUE : VK_FALSE;

            depthstencil.front.compareMask = pso->desc.dss->stencilReadMask;
            depthstencil.front.writeMask = pso->desc.dss->stencilWriteMask;
            depthstencil.front.reference = 0; // runtime supplied
            depthstencil.front.compareOp = ConvertComparisonFunc(pso->desc.dss->frontFace.stencilFunc);
            depthstencil.front.passOp = ConvertStencilOp(pso->desc.dss->frontFace.stencilPassOp);
            depthstencil.front.failOp = ConvertStencilOp(pso->desc.dss->frontFace.stencilFailOp);
            depthstencil.front.depthFailOp = ConvertStencilOp(pso->desc.dss->frontFace.stencilDepthFailOp);

            depthstencil.back.compareMask = pso->desc.dss->stencilReadMask;
            depthstencil.back.writeMask = pso->desc.dss->stencilWriteMask;
            depthstencil.back.reference = 0; // runtime supplied
            depthstencil.back.compareOp = ConvertComparisonFunc(pso->desc.dss->backFace.stencilFunc);
            depthstencil.back.passOp = ConvertStencilOp(pso->desc.dss->backFace.stencilPassOp);
            depthstencil.back.failOp = ConvertStencilOp(pso->desc.dss->backFace.stencilFailOp);
            depthstencil.back.depthFailOp = ConvertStencilOp(pso->desc.dss->backFace.stencilDepthFailOp);
        }

        // Primitive type:
        VkPipelineInputAssemblyStateCreateInfo& input_assembly = internal_state->inputAssembly;
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        switch (pso->desc.pt)
        {
        case PrimitiveTopology::PointList:
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
        case PrimitiveTopology::LineList:
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case PrimitiveTopology::LineStrip:
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            break;
        case PrimitiveTopology::TriangleStrip:
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            break;
        case PrimitiveTopology::TriangleList:
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        default:
            break;
        }
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Rasterizer:
        VkPipelineRasterizationStateCreateInfo& rasterizer = internal_state->rasterizer;
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 5.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        if (pso->desc.rs)
        {
            const RasterizerState& rs = *pso->desc.rs;

            switch (rs.polygonMode)
            {
            case PolygonMode::Fill:
                rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
                break;
            case PolygonMode::Line:
                rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
                break;
            case PolygonMode::Point:
                rasterizer.polygonMode = VK_POLYGON_MODE_POINT;
                break;
            default: assert(0); break;
            }

            switch (rs.cullMode)
            {
            case CullMode::Front:
                rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
                break;
            case CullMode::Back:
                rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
                break;
            case CullMode::None:
            default:
                rasterizer.cullMode = VK_CULL_MODE_NONE;
                break;
            }

            switch (rs.frontFace)
            {
            case FrontFace::CW:
                rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
                break;
            case FrontFace::CCW:
            default:
                rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
                break;
            }

            rasterizer.lineWidth = rs.lineWidth;
        }

        // Add shaders:
        uint32_t shaderStageCount = 0;
        auto validate_and_add_shadder = [&] (const Shader* shader) {
            if (shader != nullptr && shader->IsValid())
                internal_state->shaderStages[shaderStageCount++] = ToInternal(shader)->stageInfo;
        };

        validate_and_add_shadder(pso->desc.vs);
        validate_and_add_shadder(pso->desc.gs);
        validate_and_add_shadder(pso->desc.ps);
        if (shaderStageCount == 0)
        {
            CYB_ERROR("Pipeline has no valid shader attached!");
            return false;
        }

        // Setup pipeline create info:
        VkGraphicsPipelineCreateInfo& pipeline_info = internal_state->pipelineInfo;
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = shaderStageCount;
        pipeline_info.pStages = internal_state->shaderStages;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewportState;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pDepthStencilState = &depthstencil;
        pipeline_info.layout = internal_state->pipelineLayout;

        pipeline_info.pDynamicState = &dynamic_state_info;

        return true;
    }

    void GraphicsDevice_Vulkan::BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd)
    {
        assert(rects != nullptr);
        VkRect2D scissors[16];
        assert(rectCount < _countof(scissors));
        assert(rectCount < properties2.properties.limits.maxViewports);
        for (uint32_t i = 0; i < rectCount; ++i)
        {
            scissors[i].extent.width = abs(rects[i].right - rects[i].left);
            scissors[i].extent.height = abs(rects[i].top - rects[i].bottom);
            scissors[i].offset.x = std::max(0, rects[i].left);
            scissors[i].offset.y = std::max(0, rects[i].top);
        }
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdSetScissorWithCount(commandlist.GetCommandBuffer(), rectCount, scissors);
    }

    void GraphicsDevice_Vulkan::BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd)
    {
        assert(viewports != nullptr);
        VkViewport vp[16];
        assert(viewportCount < _countof(vp));
        assert(viewportCount < properties2.properties.limits.maxViewports);

        for (uint32_t i = 0; i < viewportCount; ++i)
        {
            vp[i].x = viewports[i].x;
            vp[i].y = viewports[i].y + viewports[i].height;
            vp[i].width = viewports[i].width;
            vp[i].height = -viewports[i].height;
            vp[i].minDepth = viewports[i].minDepth;
            vp[i].maxDepth = viewports[i].maxDepth;
        }
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdSetViewport(commandlist.GetCommandBuffer(), 0, viewportCount, vp);
    }

    void GraphicsDevice_Vulkan::BindPipelineState(const PipelineState* pso, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);

        size_t pipelineHash = 0;
        HashCombine(pipelineHash, pso->hash);
        HashCombine(pipelineHash, commandlist.renderpassInfo.GetHash());
        if (pipelineHash == commandlist.prevPipelineHash)
            return;

        commandlist.prevPipelineHash = pipelineHash;
        commandlist.dirty_pso = true;

        auto internal_state = ToInternal(pso);

        if (commandlist.active_pso == nullptr)
        {
            commandlist.binder.dirtyFlags |= DescriptorBinder::DIRTY_ALL;
        }
        else
        {
            auto active_internal = ToInternal(commandlist.active_pso);
            if (internal_state->binding_hash != active_internal->binding_hash)
                commandlist.binder.dirtyFlags |= DescriptorBinder::DIRTY_ALL;
        }

        commandlist.active_pso = pso;
    }

    CommandList GraphicsDevice_Vulkan::BeginCommandList(QueueType queue)
    {
        m_cmdLocker.Lock();
        const uint32_t cmd_current = m_cmdCount++;
        if (cmd_current >= m_commandlists.size())
            m_commandlists.push_back(std::make_unique<CommandList_Vulkan>());
        CommandList cmd;
        cmd.internal_state = m_commandlists[cmd_current].get();
        m_cmdLocker.Unlock();

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.Reset(GetBufferIndex());
        commandlist.queue = queue;

        if (commandlist.GetCommandBuffer() == VK_NULL_HANDLE)
        {
            // Need to create one more command list:
            for (uint32_t buffer_index = 0; buffer_index < BUFFERCOUNT; ++buffer_index)
            {
                VkCommandPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
                poolInfo.queueFamilyIndex = graphicsFamily;
                VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandlist.commandpools[buffer_index][static_cast<uint32_t>(queue)]));

                VkCommandBufferAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = commandlist.commandpools[buffer_index][static_cast<uint32_t>(queue)];
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandlist.commandbuffers[buffer_index][static_cast<uint32_t>(queue)]));

                commandlist.binder_pools[buffer_index].Init(this);
            }

            commandlist.binder.Init(this);
        }

        VK_CHECK(vkResetCommandPool(device, commandlist.GetCommandPool(), 0));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(commandlist.GetCommandBuffer(), &beginInfo));

        if (queue == QueueType::Graphics)
        {
            VkViewport vp = {};
            vp.width = 1;
            vp.height = 1;
            vp.maxDepth = 1;
            vkCmdSetViewportWithCount(commandlist.GetCommandBuffer(), 1, &vp);

            VkRect2D scissor;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = 65535;
            scissor.extent.height = 65535;
            vkCmdSetScissorWithCount(commandlist.GetCommandBuffer(), 1, &scissor);
            
            if (features2.features.depthBounds == VK_TRUE)
                vkCmdSetDepthBounds(commandlist.GetCommandBuffer(), 0.0f, 1.0f);
        }
        return cmd;
    }

    void GraphicsDevice_Vulkan::SetFenceName(VkFence fence, const char* name)
    {
        if (!extensions.EXT_debug_utils || fence == VK_NULL_HANDLE)
            return;

        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_FENCE;
        info.objectHandle = (uint64_t)fence;

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &info));
    }

    void GraphicsDevice_Vulkan::SetSemaphoreName(VkSemaphore semaphore, const char* name)
    {
        if (!extensions.EXT_debug_utils)
            return;

        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        info.objectHandle = (uint64_t)semaphore;

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &info));
    }

    void GraphicsDevice_Vulkan::CommandQueue::AddWaitSemaphore(VkSemaphore semaphore, uint64_t value)
    {
        VkSemaphoreSubmitInfo& waitSemaphore = submit_waitSemaphoreInfos.emplace_back();
        waitSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphore.semaphore = semaphore;
        waitSemaphore.value = value;
        waitSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

    void GraphicsDevice_Vulkan::CommandQueue::AddSignalSemaphore(VkSemaphore semaphore, uint64_t value)
    {
        VkSemaphoreSubmitInfo& signalSemaphore = submit_signalSemaphoreInfos.emplace_back();
        signalSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphore.semaphore = semaphore;
        signalSemaphore.value = value;
        signalSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

    uint64_t GraphicsDevice_Vulkan::CommandQueue::Submit(GraphicsDevice_Vulkan* device, VkFence fence)
    {
        ScopedMutex lock(*locker);

        // signal the tracking semaphore with the last submitted ID to mark 
        // the end of the frame
        lastSubmittedID++;
        AddSignalSemaphore(trackingSemaphore, lastSubmittedID);

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.commandBufferInfoCount = (uint32_t)submit_cmds.size();
        submitInfo.pCommandBufferInfos = submit_cmds.data();

        submitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(submit_waitSemaphoreInfos.size());
        submitInfo.pWaitSemaphoreInfos = submit_waitSemaphoreInfos.data();
        
        submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(submit_signalSemaphoreInfos.size());
        submitInfo.pSignalSemaphoreInfos = submit_signalSemaphoreInfos.data();

        VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, fence));

        submit_waitSemaphoreInfos.clear();
        submit_signalSemaphoreInfos.clear();
        submit_cmds.clear();

        if (!swapchains.empty())
        {
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = static_cast<uint32_t>(swapchainWaitSemaphores.size());
            presentInfo.pWaitSemaphores = swapchainWaitSemaphores.data();
            presentInfo.swapchainCount = static_cast<uint32_t>(swapchains.size());
            presentInfo.pSwapchains = swapchains.data();
            presentInfo.pImageIndices = swapchainImageIndices.data();
            VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

            swapchainWaitSemaphores.clear();
            swapchains.clear();
            swapchainImageIndices.clear();
        }

        return lastSubmittedID;
    }

    GraphicsDevice_Vulkan::CommandQueue& GraphicsDevice_Vulkan::GetQueue(QueueType queueType)
    {
        assert(queueType < QueueType::Count);
        return queues[Numerical(queueType)];
    }

    void GraphicsDevice_Vulkan::ExecuteCommandLists()
    {
        // Submit current frame:
        {
            const uint32_t cmd_last = m_cmdCount;
            m_cmdCount = 0;

            for (uint32_t cmd_index = 0; cmd_index < cmd_last; ++cmd_index)
            {
                CommandList_Vulkan& commandlist = *m_commandlists[cmd_index].get();
                VK_CHECK(vkEndCommandBuffer(commandlist.GetCommandBuffer()));

                CommandQueue& queue = GetQueue(commandlist.queue);

                VkCommandBufferSubmitInfo& submitInfo = queue.submit_cmds.emplace_back();
                submitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                submitInfo.commandBuffer = commandlist.GetCommandBuffer();

                for (auto& swapchain : commandlist.prevSwapchains)
                {
                    auto internal_state = ToInternal(&swapchain);

                    VkSemaphoreSubmitInfo& waitSemaphore = queue.submit_waitSemaphoreInfos.emplace_back();
                    waitSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    waitSemaphore.semaphore = internal_state->swapchainAcquireSemaphores[internal_state->swapchainAcquireSemaphoreIndex];
                    waitSemaphore.value = 0; // not a timeline semaphore
                    waitSemaphore.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

                    VkSemaphoreSubmitInfo& signalSemaphore = queue.submit_signalSemaphoreInfos.emplace_back();
                    signalSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    signalSemaphore.semaphore = internal_state->swapchainReleaseSemaphore;
                    signalSemaphore.value = 0; // not a timeline semaphore
                    signalSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

                    queue.swapchains.push_back(internal_state->swapchain);
                    queue.swapchainImageIndices.push_back(internal_state->swapchainImageIndex);
                    queue.swapchainWaitSemaphores.push_back(signalSemaphore.semaphore);
                }

                for (auto& x : commandlist.pipelinesWorker)
                {
                    if (m_pipelinesGlobal.count(x.first) == 0)
                    {
                        m_pipelinesGlobal[x.first] = x.second;
                    }
                    else
                    {
                        m_allocationHandler->destroylocker.Lock();
                        m_allocationHandler->destroyer_pipelines.push_back(std::make_pair(x.second, frameCount));
                        m_allocationHandler->destroylocker.Unlock();
                    }
                }
                commandlist.pipelinesWorker.clear();
            }

            for (auto& queue : queues)
                queue.Submit(this, nullptr);
        }

        // Sync up every queue to every other queue at the end of the frame.
        // NOTES: 
        //  - it disables overlapping queues into the next frame
        //  - it's not submitted immediately here, but the waits are recorded before next frame submits
        for (int queue1 = 0; queue1 < Numerical(QueueType::Count); ++queue1)
        {
            if (queues[queue1].queue == nullptr)
                continue;
            for (int queue2 = 0; queue2 < Numerical(QueueType::Count); ++queue2)
            {
                if (queue1 == queue2)
                    continue;

                queues[queue1].AddWaitSemaphore(queues[queue2].trackingSemaphore, queues[queue2].lastSubmittedID);
            }
        }

        frameCount++;

        // Begin next frame:
        {
            if (frameCount >= BUFFERCOUNT)
            {
                VkSemaphore waitSemaphores[Numerical(QueueType::Count)] = {};
                uint64_t waitValues[Numerical(QueueType::Count)] = {};
                uint32_t waitSemaphoreCount = 0;

                for (auto& queue : queues)
                {
                    if (queue.lastSubmittedID < BUFFERCOUNT)
                        continue;

                    waitSemaphores[waitSemaphoreCount] = queue.trackingSemaphore;
                    waitValues[waitSemaphoreCount] = queue.lastSubmittedID - BUFFERCOUNT + 1;
                    ++waitSemaphoreCount;
                }
                if (waitSemaphoreCount > 0)
                {
                    VkSemaphoreWaitInfo waitInfo = {};
                    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
                    waitInfo.semaphoreCount = waitSemaphoreCount;
                    waitInfo.pSemaphores = waitSemaphores;
                    waitInfo.pValues = waitValues;

                    while (VK_CHECK(vkWaitSemaphores(device, &waitInfo, timeoutValue)) == VK_TIMEOUT)
                    {
                        CYB_ERROR("[SubmitCommandLists] vkWaitSemaphores resulted in VK_TIMEOUT");
                        std::this_thread::yield();
                    }
                }
            }
        }

        m_allocationHandler->Update(frameCount, BUFFERCOUNT);
    }

    void GraphicsDevice_Vulkan::WaitForGPU() const
    {
        VK_CHECK(vkDeviceWaitIdle(device));
    }

    void GraphicsDevice_Vulkan::ClearPipelineStateCache()
    {
        m_allocationHandler->destroylocker.Lock();

        m_psoLayoutCacheMutex.Lock();
        for (auto& it : m_psoLayoutCache)
        {
            if (it.second.pipeline_layout)
                m_allocationHandler->destroyer_pipelineLayouts.push_back(std::make_pair(it.second.pipeline_layout, frameCount));
            if (it.second.descriptorset_layout)
                m_allocationHandler->destroyer_descriptorSetLayouts.push_back(std::make_pair(it.second.descriptorset_layout, frameCount));

        }
        m_psoLayoutCache.clear();
        m_psoLayoutCacheMutex.Unlock();

        for (auto& it : m_pipelinesGlobal)
        {
            m_allocationHandler->destroyer_pipelines.push_back(std::make_pair(it.second, frameCount));
        }
        m_pipelinesGlobal.clear();

        for (auto& x : m_commandlists)
        {
            for (auto& y : x->pipelinesWorker)
                m_allocationHandler->destroyer_pipelines.push_back(std::make_pair(y.second, frameCount));
            x->pipelinesWorker.clear();
        }
        m_allocationHandler->destroylocker.Unlock();

        // Destroy vulkan pipeline cache
        vkDestroyPipelineCache(device, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;

        // Create Vulkan pipeline cache
        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.initialDataSize = 0;
        createInfo.pInitialData = nullptr;
        VK_CHECK(vkCreatePipelineCache(device, &createInfo, nullptr, &m_pipelineCache));
    }

    void GraphicsDevice_Vulkan::BeginRenderPass(const Swapchain* swapchain, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.renderpassBarriersBegin.clear();
        commandlist.renderpassBarriersEnd.clear();
        auto internal_state = ToInternal(swapchain);

        internal_state->swapchainAcquireSemaphoreIndex = (internal_state->swapchainAcquireSemaphoreIndex + 1) % internal_state->swapchainAcquireSemaphores.size();

        internal_state->locker.Lock();
        VkResult res = vkAcquireNextImageKHR(
            device,
            internal_state->swapchain,
            UINT64_MAX,
            internal_state->swapchainAcquireSemaphores[internal_state->swapchainAcquireSemaphoreIndex],
            VK_NULL_HANDLE,
            &internal_state->swapchainImageIndex);
        internal_state->locker.Unlock();

        if (res != VK_SUCCESS)
        {
            // Handle outdated error in acquire:
            if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                // we need to create a new semaphore or jump through a few hoops to
                // wait for the current one to be unsignalled before we can use it again
                // creating a new one is easiest. See also:
                // https://github.com/KhronosGroup/Vulkan-Docs/issues/152
                // https://www.khronos.org/blog/resolving-longstanding-issues-with-wsi
                {
                    ScopedMutex lock(m_allocationHandler->destroylocker);
                    for (auto& x : internal_state->swapchainAcquireSemaphores)
                    {
                        m_allocationHandler->destroyer_semaphores.emplace_back(x, m_allocationHandler->framecount);
                    }
                }
                internal_state->swapchainAcquireSemaphores.clear();
                if (CreateSwapchainInternal(internal_state, physicalDevice, device, m_allocationHandler))
                {
                    BeginRenderPass(swapchain, cmd);
                    return;
                }
            }
            assert(0);
        }
        commandlist.prevSwapchains.push_back(*swapchain);

        VkRenderingInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        info.renderArea.offset.x = 0;
        info.renderArea.offset.y = 0;
        info.renderArea.extent.width = swapchain->desc.width;
        info.renderArea.extent.height = swapchain->desc.height;
        info.layerCount = 1;

        VkRenderingAttachmentInfo color_attachment = {};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = internal_state->swapchainImageViews[internal_state->swapchainImageIndex];
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue.color.float32[0] = swapchain->desc.clearColor[0];
        color_attachment.clearValue.color.float32[1] = swapchain->desc.clearColor[1];
        color_attachment.clearValue.color.float32[2] = swapchain->desc.clearColor[2];
        color_attachment.clearValue.color.float32[3] = swapchain->desc.clearColor[3];

        info.colorAttachmentCount = 1;
        info.pColorAttachments = &color_attachment;

        VkImageMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image = internal_state->swapchainImages[internal_state->swapchainImageIndex];
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        barrier.srcAccessMask = VK_ACCESS_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandlist.GetCommandBuffer(), &dependencyInfo);

        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_NONE;
        commandlist.renderpassBarriersEnd.push_back(barrier);

        vkCmdBeginRendering(commandlist.GetCommandBuffer(), &info);

        commandlist.renderpassInfo = RenderPassInfo::GetFrom(swapchain->desc);
    }

    void GraphicsDevice_Vulkan::BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd)
    {
        assert(images != nullptr);
        assert(imageCount > 0);
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.renderpassBarriersBegin.clear();
        commandlist.renderpassBarriersEnd.clear();

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.layerCount = 1;
        renderingInfo.renderArea.offset.x = 0;
        renderingInfo.renderArea.offset.y = 0;

        VkRenderingAttachmentInfo colorAttachments[8] = {};
        VkRenderingAttachmentInfo depthAttachment = {};
        VkRenderingAttachmentInfo stencilAttachment = {};
        bool hasColor = false;
        bool hasDepth = false;
        bool hasStencil = false;

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            const RenderPassImage& image = images[i];
            const Texture* texture = image.texture;
            Texture_Vulkan* textureInternal = ToInternal(texture);

            renderingInfo.renderArea.extent.width = std::max(renderingInfo.renderArea.extent.width, texture->desc.width);
            renderingInfo.renderArea.extent.height = std::max(renderingInfo.renderArea.extent.height, texture->desc.height);

            VkAttachmentLoadOp loadOp = ConvertLoadOp(image.loadOp);
            VkAttachmentStoreOp storeOp = ConvertStoreOp(image.storeOp);

            const FormatInfo& formatInfo = GetFormatInfo(texture->desc.format);

            switch (image.type)
            {
            case RenderPassImage::Type::RenderTarget: {
                VkRenderingAttachmentInfo& colorAttachment = colorAttachments[renderingInfo.colorAttachmentCount++];
                colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                colorAttachment.imageView = textureInternal->rtv.imageView;
                colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachment.loadOp = loadOp;
                colorAttachment.storeOp = storeOp;
                colorAttachment.clearValue.color.float32[0] = texture->desc.clear.color[0];
                colorAttachment.clearValue.color.float32[1] = texture->desc.clear.color[1];
                colorAttachment.clearValue.color.float32[2] = texture->desc.clear.color[2];
                colorAttachment.clearValue.color.float32[3] = texture->desc.clear.color[3];
                hasColor = true;
            } break;
            case RenderPassImage::Type::DepthStencil: {
                depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depthAttachment.imageView = textureInternal->dsv.imageView;
                if (HasFlag(image.layout, ResourceStates::DepthReadBit))
                    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
                else
                    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depthAttachment.loadOp = loadOp;
                depthAttachment.storeOp = storeOp;
                depthAttachment.clearValue.depthStencil.depth = texture->desc.clear.depthStencil.depth;
                hasDepth = true;

                if (formatInfo.hasStencil)
                {
                    stencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    stencilAttachment.imageView = textureInternal->dsv.imageView;
                    if (HasFlag(image.layout, ResourceStates::DepthReadBit))
                        stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
                    else
                        stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
                    stencilAttachment.loadOp = loadOp;
                    stencilAttachment.storeOp = storeOp;
                    stencilAttachment.clearValue.depthStencil.stencil = texture->desc.clear.depthStencil.stencil;
                    hasStencil = true;
                }
            } break;
            default: break;
            }

            if (image.prePassLayout != image.layout)
            {
                VkImageMemoryBarrier2& barrier = commandlist.renderpassBarriersBegin.emplace_back();
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.image = textureInternal->resource;
                barrier.oldLayout = ConvertImageLayout(image.prePassLayout);
                barrier.srcStageMask = ConvertStageMask(image.prePassLayout);
                barrier.srcAccessMask = ConvertAccessMask(image.prePassLayout);
                barrier.newLayout = ConvertImageLayout(image.layout);
                barrier.dstStageMask = ConvertStageMask(image.layout);
                barrier.dstAccessMask = ConvertAccessMask(image.layout);

                if (formatInfo.hasDepth)
                {
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    if (formatInfo.hasStencil)
                        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                else
                {
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            if (image.layout != image.postPassLayout)
            {
                VkImageMemoryBarrier2& barrier = commandlist.renderpassBarriersEnd.emplace_back();
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.image = textureInternal->resource;
                barrier.oldLayout = ConvertImageLayout(image.layout);
                barrier.srcStageMask = ConvertStageMask(image.layout);
                barrier.srcAccessMask = ConvertAccessMask(image.layout);
                barrier.newLayout = ConvertImageLayout(image.postPassLayout);
                barrier.dstStageMask = ConvertStageMask(image.postPassLayout);
                barrier.dstAccessMask = ConvertAccessMask(image.postPassLayout);

                if (formatInfo.hasDepth)
                {
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    if (formatInfo.hasStencil)
                        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                else
                {
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            renderingInfo.layerCount = std::min(texture->desc.arraySize, renderingInfo.layerCount);
            //renderingInfo.layerCount = std::min(texture->desc.arraySize, std::max(renderingInfo.layerCount, descriptor.sliceCount));
        }
        renderingInfo.pColorAttachments = hasColor ? colorAttachments : nullptr;
        renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;
        renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : nullptr;

        if (!commandlist.renderpassBarriersBegin.empty())
        {
            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(commandlist.renderpassBarriersBegin.size());
            dependencyInfo.pImageMemoryBarriers = commandlist.renderpassBarriersBegin.data();

            vkCmdPipelineBarrier2(commandlist.GetCommandBuffer(), &dependencyInfo);
        }

        vkCmdBeginRendering(commandlist.GetCommandBuffer(), &renderingInfo);
        commandlist.renderpassInfo = RenderPassInfo::GetFrom(images, imageCount);
    }

    void GraphicsDevice_Vulkan::EndRenderPass(CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdEndRendering(commandlist.GetCommandBuffer());

        if (!commandlist.renderpassBarriersEnd.empty())
        {
            VkDependencyInfo dependencyInfo = {};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(commandlist.renderpassBarriersEnd.size());
            dependencyInfo.pImageMemoryBarriers = commandlist.renderpassBarriersEnd.data();

            vkCmdPipelineBarrier2(commandlist.GetCommandBuffer(), &dependencyInfo);
            commandlist.renderpassBarriersEnd.clear();
        }

        commandlist.renderpassInfo = {};
    }

    void GraphicsDevice_Vulkan::Draw(uint32_t vertexCount, uint32_t startVertexLocation, CommandList cmd)
    {
        PreDraw(cmd);
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdDraw(commandlist.GetCommandBuffer(), vertexCount, 1, startVertexLocation, 0);
    }

    void GraphicsDevice_Vulkan::DrawIndexed(uint32_t index_count, uint32_t start_index_location, int32_t base_vertex_location, CommandList cmd)
    {
        PreDraw(cmd);

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdDrawIndexed(commandlist.GetCommandBuffer(), index_count, 1, start_index_location, base_vertex_location, 0);
    }

    void GraphicsDevice_Vulkan::BeginQuery(const GPUQuery* query, uint32_t index, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto internal_state = static_cast<Query_Vulkan*>(query->internal_state.get());

        switch (query->desc.type)
        {
        case GPUQueryType::OcclusionBinary:
            vkCmdBeginQuery(commandlist.GetCommandBuffer(), internal_state->pool, index, 0);
            break;
        case GPUQueryType::Occlusion:
            vkCmdBeginQuery(commandlist.GetCommandBuffer(), internal_state->pool, index, VK_QUERY_CONTROL_PRECISE_BIT);
            break;
        case GPUQueryType::Timestamp:
            break;
        }
    }

    void GraphicsDevice_Vulkan::EndQuery(const GPUQuery* query, uint32_t index, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto internal_state = static_cast<Query_Vulkan*>(query->internal_state.get());

        switch (query->desc.type)
        {
        case GPUQueryType::OcclusionBinary:
        case GPUQueryType::Occlusion:
            vkCmdEndQuery(commandlist.GetCommandBuffer(), internal_state->pool, index);
            break;
        case GPUQueryType::Timestamp:
            vkCmdWriteTimestamp2(commandlist.GetCommandBuffer(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, internal_state->pool, index);
            break;
        }
    }

    void GraphicsDevice_Vulkan::ResolveQuery(const GPUQuery* query, uint32_t index, uint32_t count, const GPUBuffer* dest, uint64_t destOffset, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto internal_state = static_cast<Query_Vulkan*>(query->internal_state.get());
        auto dst_internal = static_cast<Buffer_Vulkan*>(dest->internal_state.get());

        VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
        if (query->desc.type == GPUQueryType::OcclusionBinary)
            flags |= VK_QUERY_RESULT_PARTIAL_BIT;

        vkCmdCopyQueryPoolResults(
            commandlist.GetCommandBuffer(),
            internal_state->pool,
            index,
            count,
            dst_internal->resource,
            destOffset,
            sizeof(uint64_t),
            flags);
    }

    void GraphicsDevice_Vulkan::ResetQuery(const GPUQuery* query, uint32_t index, uint32_t count, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto internal_state = static_cast<Query_Vulkan*>(query->internal_state.get());

        vkCmdResetQueryPool(
            commandlist.GetCommandBuffer(),
            internal_state->pool,
            index,
            count);
    }

    void GraphicsDevice_Vulkan::PushConstants(const void* data, uint32_t size, CommandList cmd, uint32_t offset)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);

        if (commandlist.active_pso != nullptr)
        {
            auto pso_internal = ToInternal(commandlist.active_pso);
            if (pso_internal->pushconstants.size > 0)
            {
                vkCmdPushConstants(
                    commandlist.GetCommandBuffer(),
                    pso_internal->pipelineLayout,
                    pso_internal->pushconstants.stageFlags,
                    offset,
                    size,
                    data
                );
                return;
            }
            assert(0);  // no push constant block!
        }

        assert(0);      // no active pipeline!
    }

    void GraphicsDevice_Vulkan::SetName(GPUResource* resource, const char* name)
    {
        if (!extensions.EXT_debug_utils || resource == nullptr || !resource->IsValid())
            return;

        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.pObjectName = name;
        if (resource->IsBuffer())
        {
            info.objectType = VK_OBJECT_TYPE_BUFFER;
            info.objectHandle = (uint64_t)ToInternal((const GPUBuffer*)resource)->resource;
        }
        if (resource->IsTexture())
        {
            info.objectType = VK_OBJECT_TYPE_IMAGE;
            info.objectHandle = (uint64_t)ToInternal((const Texture*)resource)->resource;
        }

        if (info.objectHandle == (uint64_t)VK_NULL_HANDLE)
            return;

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &info));
    }

    void GraphicsDevice_Vulkan::SetName(Shader* shader, const char* name)
    {
        if (!extensions.EXT_debug_utils || shader == nullptr || !shader->IsValid())
            return;

        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
        info.objectHandle = (uint64_t)ToInternal(shader)->shadermodule;
        info.pObjectName = name;

        if (info.objectHandle == (uint64_t)VK_NULL_HANDLE)
            return;

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &info));
    }

    void GraphicsDevice_Vulkan::BeginEvent(const char* name, CommandList cmd)
    {
        if (!extensions.EXT_debug_utils)
            return;

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        const uint64_t hash = HashString(name);

        VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name;
        label.color[0] = ((hash >> 24) & 0xFF) / 255.0f;
        label.color[1] = ((hash >> 16) & 0xFF) / 255.0f;
        label.color[2] = ((hash >> 8) & 0xFF) / 255.0f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(commandlist.GetCommandBuffer(), &label);
    }

    void GraphicsDevice_Vulkan::EndEvent(CommandList cmd)
    {
        if (!extensions.EXT_debug_utils)
            return;

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdEndDebugUtilsLabelEXT(commandlist.GetCommandBuffer());
    }
}
