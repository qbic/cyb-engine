#include <unordered_set>
#include "core/logger.h"
#include "core/platform.h"
#include "core/hash.h"
#include "graphics/graphics-device-vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "spirv_reflect.h"

#define CYB_DEBUGBREAK_ON_VALIDATION_ERROR  1

namespace cyb::graphics::vulkan_internal
{
    constexpr VkFormat _ConvertFormat(Format value)
    {
        switch (value)
        {
        case Format::Unknown:                   return VK_FORMAT_UNDEFINED;
        case Format::R32G32B32A32_Float:        return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32_Float:              return VK_FORMAT_R32G32_SFLOAT;
        case Format::R8G8B8A8_Unorm:            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_Uint:             return VK_FORMAT_R8G8B8A8_UINT;
        case Format::R16G16_Float:              return VK_FORMAT_R16G16_SFLOAT;
        case Format::D32_Float:                 return VK_FORMAT_D32_SFLOAT;
        case Format::D32_Float_S8_Uint:         return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::R32_Float:                 return VK_FORMAT_R32_SFLOAT;
        case Format::R16_Float:                 return VK_FORMAT_R16_SFLOAT;
        case Format::R8_Unorm:                  return VK_FORMAT_R8_UNORM;
        case Format::B8G8R8A8_Unorm:            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::R32G32B32_Float:           return VK_FORMAT_R32G32B32_SFLOAT;
        }

        assert(0);
        return VK_FORMAT_UNDEFINED;
    }

    constexpr VkComponentSwizzle _ConvertComponentSwizzle(TextureComponentSwizzle swizzle)
    {
        switch (swizzle)
        {
        case TextureComponentSwizzle::Identity: return VK_COMPONENT_SWIZZLE_IDENTITY;
        case TextureComponentSwizzle::Zero:     return VK_COMPONENT_SWIZZLE_ZERO;
        case TextureComponentSwizzle::One:      return VK_COMPONENT_SWIZZLE_ONE;
        case TextureComponentSwizzle::R:        return VK_COMPONENT_SWIZZLE_R;
        case TextureComponentSwizzle::G:        return VK_COMPONENT_SWIZZLE_G;
        case TextureComponentSwizzle::B:        return VK_COMPONENT_SWIZZLE_B;
        case TextureComponentSwizzle::A:        return VK_COMPONENT_SWIZZLE_A;
        }

        assert(0);
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    constexpr VkCompareOp _ConvertComparisonFunc(ComparisonFunc value)
    {
        switch (value)
        {
        case ComparisonFunc::Never:             return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:              return VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:             return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessEqual:         return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:           return VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:          return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual:      return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Allways:           return VK_COMPARE_OP_ALWAYS;
        }

        assert(0);
        return VK_COMPARE_OP_NEVER;
    }

    constexpr VkStencilOp _ConvertStencilOp(StencilOp value)
    {
        switch (value)
        {
        case StencilOp::Keep:                  return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero:                  return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace:               return VK_STENCIL_OP_REPLACE;
        case StencilOp::IncrementClamp:        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::DecrementClamp:        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::Invert:                return VK_STENCIL_OP_INVERT;
        case StencilOp::Increment:             return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::Decrement:             return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }

        assert(0);
        return VK_STENCIL_OP_KEEP;
    }

    constexpr VkAttachmentLoadOp _ConvertLoadOp(RenderPassAttachment::LoadOp loadOp)
    {
        switch (loadOp)
        {
        case RenderPassAttachment::LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RenderPassAttachment::LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RenderPassAttachment::LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }

        assert(0);
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
 
    constexpr VkAttachmentStoreOp _ConvertStoreOp(RenderPassAttachment::StoreOp storeOp)
    {
        switch (storeOp)
        {
        case RenderPassAttachment::StoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case RenderPassAttachment::StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }

        assert(0);
        return VK_ATTACHMENT_STORE_OP_STORE;
    }

    constexpr VkImageLayout _ConvertImageLayout(ResourceState value)
    {
        switch (value)
        {
        case ResourceState::Undefined:              return VK_IMAGE_LAYOUT_UNDEFINED;
        case ResourceState::RenderTargetBit:        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ResourceState::DepthStencilBit:        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ResourceState::DepthStencil_ReadOnlyBit: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ResourceState::ShaderResourceBit:
        case ResourceState::ShaderResourceComputeBit: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ResourceState::UnorderedAccessBit:     return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::CopySrcBit:             return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ResourceState::CopyDstBit:             return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        assert(0);
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    constexpr VkAccessFlags _ParseResourceState(ResourceState value)
    {
        VkAccessFlags flags = 0;

        if (HasFlag(value, ResourceState::ShaderResourceBit))
            flags |= VK_ACCESS_SHADER_READ_BIT;
        if (HasFlag(value, ResourceState::ShaderResourceComputeBit))
            flags |= VK_ACCESS_SHADER_READ_BIT;
        if (HasFlag(value, ResourceState::UnorderedAccessBit))
        {
            flags |= VK_ACCESS_SHADER_READ_BIT;
            flags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        if (HasFlag(value, ResourceState::CopySrcBit))
            flags |= VK_ACCESS_TRANSFER_READ_BIT;
        if (HasFlag(value, ResourceState::CopyDstBit))
            flags |= VK_ACCESS_TRANSFER_WRITE_BIT;

        if (HasFlag(value, ResourceState::RenderTargetBit))
        {
            flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (HasFlag(value, ResourceState::DepthStencilBit))
        {
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (HasFlag(value, ResourceState::DepthStencil_ReadOnlyBit))
            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        if (HasFlag(value, ResourceState::VertexBufferBit))
            flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        if (HasFlag(value, ResourceState::IndexBufferBit))
            flags |= VK_ACCESS_INDEX_READ_BIT;
        if (HasFlag(value, ResourceState::ConstantBufferBit))
            flags |= VK_ACCESS_UNIFORM_READ_BIT;
        if (HasFlag(value, ResourceState::IndirectArgumentBit))
            flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        if (HasFlag(value, ResourceState::PredictionBit))
            flags |= VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT;

        return flags;
    }

    struct Buffer_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VmaAllocation allocation = nullptr;
        VkBuffer resource;

        ~Buffer_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            allocationhandler->destroyer_buffers.push_back(std::make_pair(std::make_pair(resource, allocation), framecount));
            allocationhandler->destroylocker.unlock();
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
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            if (pool) allocationhandler->destroyer_querypools.push_back(std::make_pair(pool, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    struct Texture_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VmaAllocation allocation = nullptr;
        VkImage resource = VK_NULL_HANDLE;

        struct TextureSubresource
        {
            VkImageView image_view = VK_NULL_HANDLE;
            // NOTE: room for bindless index
        };

        TextureSubresource srv;
        VkImageView rtv = VK_NULL_HANDLE;
        VkImageView dsv = VK_NULL_HANDLE;

        ~Texture_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            if (srv.image_view) allocationhandler->destroyer_imageviews.push_back(std::make_pair(srv.image_view, framecount));
            if (resource) allocationhandler->destroyer_images.push_back(std::make_pair(std::make_pair(resource, allocation), framecount));
            if (rtv) allocationhandler->destroyer_imageviews.push_back(std::make_pair(rtv, framecount));
            if (dsv) allocationhandler->destroyer_imageviews.push_back(std::make_pair(dsv, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    struct Shader_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkShaderModule shadermodule = VK_NULL_HANDLE;
        VkPipelineShaderStageCreateInfo stage_info = {};

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        VkDeviceSize uniform_buffer_sizes[DESCRIPTORBINDER_CBV_COUNT] = {};
        std::vector<uint32_t> uniform_buffer_dynamic_slots;
        std::vector<VkImageViewType> imageview_types;

        ~Shader_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            if (shadermodule) allocationhandler->destroyer_shadermodules.push_back(std::make_pair(shadermodule, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    struct Sampler_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkSampler resource = VK_NULL_HANDLE;

        ~Sampler_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            if (resource) allocationhandler->destroyer_samplers.push_back(std::make_pair(resource, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    struct PipelineState_Vulkan
    {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;               // no lifetime management here
        VkDescriptorSetLayout descriptorset_layout = VK_NULL_HANDLE;    // no lifetime management here
        
        std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
        std::vector<VkImageViewType> imageview_types;
        VkDeviceSize uniform_buffer_sizes[DESCRIPTORBINDER_CBV_COUNT] = {};
        std::vector<uint32_t> uniform_buffer_dynamic_slots;
        size_t binding_hash = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        VkPipelineShaderStageCreateInfo shaderStages[static_cast<size_t>(ShaderStage::_Count)] = {};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        VkPipelineRasterizationDepthClipStateCreateInfoEXT depthclip = {};
        VkViewport viewport = {};
        VkRect2D scissor = {};
        VkPipelineViewportStateCreateInfo viewportState = {};
        VkPipelineDepthStencilStateCreateInfo depthstencil = {};
    };

    struct RenderPass_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkRenderPass renderpass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkRenderPassBeginInfo begin_info = {};
        VkClearValue clear_values[9] = {};

        ~RenderPass_Vulkan()
        {
            assert(allocationhandler != nullptr);
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;
            if (renderpass) allocationhandler->destroyer_renderpasses.push_back(std::make_pair(renderpass, framecount));
            if (framebuffer) allocationhandler->destroyer_framebuffers.push_back(std::make_pair(framebuffer, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    struct SwapChain_Vulkan
    {
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationhandler;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat image_format;
        VkExtent2D extent;
        std::vector<VkImage> images;
        std::vector<VkImageView> imageviews;
        std::vector<VkFramebuffer> framebuffers;

        RenderPass renderpass;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        uint32_t image_index = 0;
        VkSemaphore semaphore_aquire = VK_NULL_HANDLE;
        VkSemaphore semaphore_release = VK_NULL_HANDLE;

        SwapChainDesc desc;

        ~SwapChain_Vulkan()
        {
            if (allocationhandler == nullptr)
                return;
            allocationhandler->destroylocker.lock();
            uint64_t framecount = allocationhandler->framecount;

            for (auto framebuffer : framebuffers)
            {
                allocationhandler->destroyer_framebuffers.push_back(std::make_pair(framebuffer, framecount));
            }

            for (auto imageView : imageviews)
            {
                allocationhandler->destroyer_imageviews.push_back(std::make_pair(imageView, framecount));
            }

            allocationhandler->destroyer_swapchains.push_back(std::make_pair(swapchain, framecount));
            allocationhandler->destroyer_surfaces.push_back(std::make_pair(surface, framecount));
            allocationhandler->destroyer_semaphores.push_back(std::make_pair(semaphore_aquire, framecount));
            allocationhandler->destroyer_semaphores.push_back(std::make_pair(semaphore_release, framecount));
            allocationhandler->destroylocker.unlock();
        }
    };

    Buffer_Vulkan* ToInternal(const GPUBuffer* param)
    {
        return static_cast<Buffer_Vulkan*>(param->internal_state.get());
    }

    Texture_Vulkan* ToInternal(const Texture* param)
    {
        return static_cast<Texture_Vulkan*>(param->internal_state.get());
    }

    Shader_Vulkan* ToInternal(const Shader* param)
    {
        return static_cast<Shader_Vulkan*>(param->internal_state.get());
    }

    Sampler_Vulkan* ToInternal(const Sampler* param)
    {
        return static_cast<Sampler_Vulkan*>(param->internal_state.get());
    }

    PipelineState_Vulkan* ToInternal(const PipelineState* param)
    {
        return static_cast<PipelineState_Vulkan*>(param->internal_state.get());
    }

    RenderPass_Vulkan* ToInternal(const RenderPass* param)
    {
        return static_cast<RenderPass_Vulkan*>(param->internal_state.get());
    }

    SwapChain_Vulkan* ToInternal(const SwapChain* param)
    {
        return static_cast<SwapChain_Vulkan*>(param->internal_state.get());
    }

    bool CheckExtensionSupport(
        const char* checkExtension, 
        const std::vector<VkExtensionProperties>& availableExtensions)
    {
        for (const auto& x : availableExtensions)
        {
            if (strcmp(x.extensionName, checkExtension) == 0)
            {
                return true;
            }
        }

        return false;
    }

    bool ValidateLayers(
        const std::vector<const char*>& required,
        const std::vector<VkLayerProperties>& available)
    {
        for (auto layer : required)
        {
            bool found = false;
            for (auto& availableLayer : available)
            {
                if (strcmp(availableLayer.layerName, layer) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return false;
            }
        }

        return true;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        CYB_WARNING("Vulkan {0}", callbackData->pMessage);
#if CYB_DEBUGBREAK_ON_VALIDATION_ERROR
        CYB_DEBUGBREAK();
#endif
        return VK_FALSE;
    }

    bool CreateSwapChain_Internal(
        SwapChain_Vulkan* internal_state,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        std::shared_ptr<GraphicsDevice_Vulkan::AllocationHandler> allocationHandler)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, internal_state->surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, internal_state->surface, &formatCount, nullptr);

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, internal_state->surface, &formatCount, formats.data());

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, internal_state->surface, &present_mode_count, nullptr);

        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, internal_state->surface, &present_mode_count, present_modes.data());

        VkSurfaceFormatKHR surfaceFormat = {};
        surfaceFormat.format = _ConvertFormat(internal_state->desc.format);
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
            internal_state->extent = capabilities.currentExtent;
        }
        else
        {
            internal_state->extent = { internal_state->desc.width, internal_state->desc.height };
            internal_state->extent.width = std::clamp(internal_state->extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            internal_state->extent.height = std::clamp(internal_state->extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = std::max(internal_state->desc.bufferCount, capabilities.minImageCount);
        if ((capabilities.maxImageCount > 0) && (imageCount > capabilities.maxImageCount))
        {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = internal_state->surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = internal_state->extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.preTransform = capabilities.currentTransform;

        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // The only one that is always supported
        if (!internal_state->desc.vsync)
        {
            for (auto& present_mode : present_modes)
            {
                if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }

        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = internal_state->swapchain;

        VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &internal_state->swapchain);
        if (res != VK_SUCCESS)
        {
            platform::CreateMessageWindow("vkCreateSwapchainKHR failed! Error: " + std::to_string(res), "Error!");
            platform::Exit(1);
        }

        if (createInfo.oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, createInfo.oldSwapchain, nullptr);
        }

        vkGetSwapchainImagesKHR(device, internal_state->swapchain, &imageCount, nullptr);
        internal_state->images.resize(imageCount);
        vkGetSwapchainImagesKHR(device, internal_state->swapchain, &imageCount, internal_state->images.data());
        internal_state->image_format = surfaceFormat.format;

        // Create default render pass:
        {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = internal_state->image_format;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            auto renderpass_internal = std::make_shared<RenderPass_Vulkan>();
            renderpass_internal->allocationhandler = allocationHandler;
            internal_state->renderpass.internal_state = renderpass_internal;
            
            vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderpass_internal->renderpass);
        }

        // Create swap chain render targets:
        internal_state->imageviews.resize(internal_state->images.size());
        internal_state->framebuffers.resize(internal_state->images.size());
        for (size_t i = 0; i < internal_state->images.size(); ++i)
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = internal_state->images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = internal_state->image_format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            res = vkCreateImageView(device, &createInfo, nullptr, &internal_state->imageviews[i]);
            if (res != VK_SUCCESS)
            {
                platform::CreateMessageWindow("vkCreateImageView failed! Error: " + std::to_string(res), "Error!");
                platform::Exit(1);
            }

            const VkImageView attachments[] = {
                internal_state->imageviews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = ToInternal(&internal_state->renderpass)->renderpass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = internal_state->extent.width;
            framebufferInfo.height = internal_state->extent.height;
            framebufferInfo.layers = 1;

            if (internal_state->framebuffers[i] != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(device, internal_state->framebuffers[i], nullptr);
            }
            
            vkCreateFramebuffer(device, &framebufferInfo, nullptr, &internal_state->framebuffers[i]);
        }

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (internal_state->semaphore_aquire == VK_NULL_HANDLE)
        {
            res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &internal_state->semaphore_aquire);
            assert(res == VK_SUCCESS);
        }

        if (internal_state->semaphore_release == VK_NULL_HANDLE)
        {
            res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &internal_state->semaphore_release);
            assert(res == VK_SUCCESS);
        }

        return true;
    }
}

using namespace cyb::graphics::vulkan_internal;

namespace cyb::graphics
{
    void GraphicsDevice_Vulkan::CopyAllocator::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;

        VkSemaphoreTypeCreateInfo timeline_info = {};
        timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.pNext = nullptr;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue = 0;

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = &timeline_info;
        semaphore_info.flags = 0;

        VkResult res = vkCreateSemaphore(device->device, &semaphore_info, nullptr, &semaphore);
        assert(res == VK_SUCCESS);
    }

    void GraphicsDevice_Vulkan::CopyAllocator::Destroy()
    {
        vkQueueWaitIdle(device->copyQueue);
        for (auto& x : freelist)
        {
            vkDestroyCommandPool(device->device, x.commandpool, nullptr);
        }
        vkDestroySemaphore(device->device, semaphore, nullptr);
    }

    GraphicsDevice_Vulkan::CopyAllocator::CopyCMD GraphicsDevice_Vulkan::CopyAllocator::Allocate(uint64_t staging_size)
    {
        locker.lock();

        // create a new command list if there are no free ones:
        if (freelist.empty())
        {
            CopyCMD cmd;

            VkCommandPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.queueFamilyIndex = device->copyFamily;
            pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

            VkResult res = vkCreateCommandPool(device->device, &pool_info, nullptr, &cmd.commandpool);
            assert(res == VK_SUCCESS);

            VkCommandBufferAllocateInfo commandbuffer_info = {};
            commandbuffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandbuffer_info.commandBufferCount = 1;
            commandbuffer_info.commandPool = cmd.commandpool;
            commandbuffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            res = vkAllocateCommandBuffers(device->device, &commandbuffer_info, &cmd.commandbuffer);
            assert(res == VK_SUCCESS);

            freelist.push_back(cmd);
        }

        CopyCMD cmd = freelist.back();
        if (cmd.uploadbuffer.desc.size < staging_size)
        {
            // Try to search for a staging buffer that can fit the request:
            for (size_t i = 0; i < freelist.size(); ++i)
            {
                if (freelist[i].uploadbuffer.desc.size >= staging_size)
                {
                    cmd = freelist[i];
                    std::swap(freelist[i], freelist.back());
                    break;
                }
            }
        }
        freelist.pop_back();
        locker.unlock();

        // If no buffer was found that fits the data, create one:
        if (cmd.uploadbuffer.desc.size < staging_size)
        {
            GPUBufferDesc uploaddesc;
            uploaddesc.size = math::GetNextPowerOfTwo(staging_size);
            uploaddesc.usage = MemoryAccess::Upload;
            bool upload_success = device->CreateBuffer(&uploaddesc, nullptr, &cmd.uploadbuffer);
            assert(upload_success);
        }

        // begin command list in valid state:
        VkResult res = vkResetCommandPool(device->device, cmd.commandpool, 0);
        assert(res == VK_SUCCESS);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        res = vkBeginCommandBuffer(cmd.commandbuffer, &beginInfo);
        assert(res == VK_SUCCESS);

        return cmd;
    }

    void GraphicsDevice_Vulkan::CopyAllocator::Submit(CopyCMD cmd)
    {
        VkResult res = vkEndCommandBuffer(cmd.commandbuffer);
        assert(res == VK_SUCCESS);

        // It was very slow in Vulkan to submit the copies immediately.
        // In Vulkan, the submit is not thread safe, so it had to be locked.
        // Instead, the submits are batched and performed in Flush() function
        locker.lock();
        cmd.target = ++fence_value;
        worklist.push_back(cmd);
        submit_cmds.push_back(cmd.commandbuffer);
        submit_wait = std::max(submit_wait, cmd.target);
        locker.unlock();
    }

    uint64_t GraphicsDevice_Vulkan::CopyAllocator::Flush()
    {
        locker.lock();
        if (!submit_cmds.empty())
        {
            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = (uint32_t)submit_cmds.size();
            submit_info.pCommandBuffers = submit_cmds.data();
            submit_info.pSignalSemaphores = &semaphore;
            submit_info.signalSemaphoreCount = 1;

            VkTimelineSemaphoreSubmitInfo timeline_info = {};
            timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timeline_info.pNext = nullptr;
            timeline_info.waitSemaphoreValueCount = 0;
            timeline_info.pWaitSemaphoreValues = nullptr;
            timeline_info.signalSemaphoreValueCount = 1;
            timeline_info.pSignalSemaphoreValues = &submit_wait;

            submit_info.pNext = &timeline_info;

            VkResult res = vkQueueSubmit(device->copyQueue, 1, &submit_info, VK_NULL_HANDLE);
            assert(res == VK_SUCCESS);

            submit_cmds.clear();
        }

        // free up the finished command lists:
        uint64_t completed_fence_value;
        VkResult res = vkGetSemaphoreCounterValue(device->device, semaphore, &completed_fence_value);
        assert(res == VK_SUCCESS);
        for (size_t i = 0; i < worklist.size(); ++i)
        {
            if (worklist[i].target <= completed_fence_value)
            {
                freelist.push_back(worklist[i]);
                worklist[i] = worklist.back();
                worklist.pop_back();
                i--;
            }
        }

        uint64_t value = submit_wait;
        submit_wait = 0;
        locker.unlock();

        return value;
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;

        descriptor_writes.reserve(128);
        buffer_infos.reserve(128);
        image_infos.reserve(128);
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Reset()
    {
        table = {};
        dirty = true;
    }

    void GraphicsDevice_Vulkan::DescriptorBinder::Flush(CommandList cmd)
    {
        if (dirty == DIRTY_NONE)
            return;

        CommandList_Vulkan& commandlist = device->GetCommandList(cmd);
        auto pso_internal = ToInternal(commandlist.active_pso);
        if (pso_internal->layout_bindings.empty())
            return;

        VkCommandBuffer commandbuffer = commandlist.GetCommandBuffer();

        VkPipelineLayout pipeline_layout = pso_internal->pipelineLayout;
        VkDescriptorSetLayout descriptorset_layout = pso_internal->descriptorset_layout;
        VkDescriptorSet descriptorset = descriptorset_graphics;
        uint32_t uniform_buffer_dynamic_count = (uint32_t)pso_internal->uniform_buffer_dynamic_slots.size();
        for (size_t i = 0; i < pso_internal->uniform_buffer_dynamic_slots.size(); ++i)
        {
            uniform_buffer_dynamic_offsets[i] = (uint32_t)table.CBV_offset[pso_internal->uniform_buffer_dynamic_slots[i]];
        }

        if (dirty & DIRTY_DESCRIPTOR)
        {
            auto& binder_pool = commandlist.binder_pools[device->GetBufferIndex()];

            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = binder_pool.descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &pso_internal->descriptorset_layout;

            VkResult res = vkAllocateDescriptorSets(device->device, &alloc_info, &descriptorset);
            while (res == VK_ERROR_OUT_OF_POOL_MEMORY)
            {
                binder_pool.pool_max_size *= 2;
                binder_pool.Destroy();
                binder_pool.Init(device);
                alloc_info.descriptorPool = binder_pool.descriptor_pool;
                res = vkAllocateDescriptorSets(device->device, &alloc_info, &descriptorset);
            }
            assert(res == VK_SUCCESS);


            descriptor_writes.clear();
            buffer_infos.clear();
            image_infos.clear();

            const auto& layout_bindings = pso_internal->layout_bindings;
            const auto& imageview_types = pso_internal->imageview_types;

            int i = 0;
            for (const auto& x : layout_bindings)
            {
                VkImageViewType viewtype = imageview_types[i++];

                for (uint32_t descriptor_index = 0; descriptor_index < x.descriptorCount; ++descriptor_index)
                {
                    uint32_t unrolled_binding = x.binding + descriptor_index;

                    auto& write = descriptor_writes.emplace_back();
                    write = {};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = descriptorset;
                    write.dstArrayElement = descriptor_index;
                    write.descriptorType = x.descriptorType;
                    write.dstBinding = x.binding;
                    write.descriptorCount = 1;

                    switch (write.descriptorType)
                    {
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    {
                        image_infos.emplace_back();
                        write.pImageInfo = &image_infos.back();
                        const uint32_t binding_location = unrolled_binding;
                        const GPUResource& resource = table.SRV[unrolled_binding];
                        auto texture_internal = ToInternal((const Texture*)&resource);
                        const Sampler& sampler = table.SAM[unrolled_binding];
                        auto sampler_internal = ToInternal((const Sampler*)&sampler);
                        image_infos.back().sampler = sampler_internal->resource;
                        image_infos.back().imageView = texture_internal->srv.image_view;
                        image_infos.back().imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    } break;

                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    {
                        buffer_infos.emplace_back();
                        write.pBufferInfo = &buffer_infos.back();
                        write.pBufferInfo = {};
                        const uint32_t binding_location = unrolled_binding;
                        const GPUBuffer& buffer = table.CBV[binding_location];
                        assert(buffer.IsBuffer() && "No buffer bound to slot");
                        uint64_t offset = table.CBV_offset[binding_location];

                        auto internal_state = ToInternal(&buffer);
                        buffer_infos.back().buffer = internal_state->resource;
                        buffer_infos.back().offset = offset;
                        buffer_infos.back().range = pso_internal->uniform_buffer_sizes[binding_location];
                        if (buffer_infos.back().range == 0ull)
                            buffer_infos.back().range = VK_WHOLE_SIZE;
                    } break;

                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    {
                        buffer_infos.emplace_back();
                        write.pBufferInfo = &buffer_infos.back();
                        buffer_infos.back() = {};

                        const uint32_t binding_location = unrolled_binding;
                        const GPUBuffer& buffer = table.CBV[binding_location];
                        assert(buffer.IsBuffer());

                        auto internal_state = ToInternal(&buffer);
                        buffer_infos.back().buffer = internal_state->resource;
                        buffer_infos.back().range = pso_internal->uniform_buffer_sizes[binding_location];
                        if (buffer_infos.back().range == 0ull)
                            buffer_infos.back().range = VK_WHOLE_SIZE;
                    } break;

                    default: assert(0);
                    }
                }
            }

            vkUpdateDescriptorSets(
                device->device,
                (uint32_t)descriptor_writes.size(),
                descriptor_writes.data(),
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
            uniform_buffer_dynamic_offsets);

        descriptorset_graphics = descriptorset;
        dirty = DIRTY_NONE;
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Init(GraphicsDevice_Vulkan* device)
    {
        this->device = device;

        // Create descriptor pool:
        std::vector<VkDescriptorPoolSize> pool_sizes = {};
        VkDescriptorPoolSize pool_size = {};

        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = DESCRIPTORBINDER_CBV_COUNT * pool_max_size;
        pool_sizes.push_back(pool_size);

        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        pool_size.descriptorCount = DESCRIPTORBINDER_CBV_COUNT * pool_max_size;
        pool_sizes.push_back(pool_size);

        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = DESCRIPTORBINDER_SRV_COUNT * pool_max_size;
        pool_sizes.push_back(pool_size);

        VkDescriptorPoolCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        create_info.poolSizeCount = (uint32_t)pool_sizes.size();
        create_info.pPoolSizes = pool_sizes.data();
        create_info.maxSets = pool_max_size;

        VkResult res = vkCreateDescriptorPool(device->device, &create_info, nullptr, &descriptor_pool);
        assert(res == VK_SUCCESS);
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Destroy()
    {
        if (descriptor_pool != VK_NULL_HANDLE)
        {
            device->allocationhandler->destroylocker.lock();
            device->allocationhandler->destroyer_descriptorPools.push_back(std::make_pair(descriptor_pool, device->frameCount));
            descriptor_pool = VK_NULL_HANDLE;
            device->allocationhandler->destroylocker.unlock();
        }
    }

    void GraphicsDevice_Vulkan::DescriptorBinderPool::Reset()
    {
        if (descriptor_pool != VK_NULL_HANDLE)
        {
            VkResult res = vkResetDescriptorPool(device->device, descriptor_pool, 0);
            assert(res == VK_SUCCESS);
        }
    }

    void GraphicsDevice_Vulkan::ValidatePSO(CommandList cmds)
    {
        CommandList_Vulkan& commandList = GetCommandList(cmds);
        if (!commandList.dirty_pso)
            return;

        const PipelineState* pso = commandList.active_pso;
        size_t pipeline_hash = commandList.prev_pipeline_hash;
        hash::HashCombine(pipeline_hash, commandList.vertexbuffer_hash);
        auto internal_state = ToInternal(pso);

        VkPipeline pipeline = VK_NULL_HANDLE;
        auto it = pipelines_global.find(pipeline_hash);
        if (it == pipelines_global.end())
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
                    bind.stride = commandList.vertexbuffer_strides[x.inputSlot];
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
                    attr.format = _ConvertFormat(x.format);
                    attr.location = i;

                    attr.offset = x.alignedByteOffset;
                    if (attr.offset == VertexInputLayout::AppendAlignedElement)
                    {
                        // need to manually resolve this from the format spec.
                        attr.offset = offset;
                        offset += GetFormatStride(x.format);
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
            VkGraphicsPipelineCreateInfo pipeline_info = internal_state->pipelineInfo; // make a copy here
            pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_info.renderPass = ToInternal(commandList.active_renderpass)->renderpass;
            pipeline_info.subpass = 0;
            pipeline_info.pMultisampleState = &multisampling;
            pipeline_info.pColorBlendState = &colorBlending;
            pipeline_info.pVertexInputState = &vertexInputInfo;

            VkResult res = vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipeline_info, nullptr, &pipeline);
            assert(res == VK_SUCCESS);

            pipelines_global.insert(std::make_pair(pipeline_hash, pipeline));
        }
        else
        {
            pipeline = it->second;
        }

        vkCmdBindPipeline(commandList.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        commandList.dirty_pso = false;
    }

    void GraphicsDevice_Vulkan::PreDraw(CommandList cmd)
    {
        ValidatePSO(cmd);
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.binder.Flush(cmd);
    }

    GraphicsDevice_Vulkan::GraphicsDevice_Vulkan()
    {
        VkResult res = volkInitialize();
        if (res != VK_SUCCESS)
        {
            platform::CreateMessageWindow("volkInitialize failed! Error: " + std::to_string(res), "Error!");
            platform::Exit(1);
        }

        // Enumerate available layers and extensions:
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data());

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data());

        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        for (auto& availableExtension : availableInstanceExtensions)
        {
            if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            {
                debugUtils = true;
                instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else if (strcmp(availableExtension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
            {
                instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            }
            else if (strcmp(availableExtension.extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0)
            {
                instanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
            }
        }

        instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
        instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

        if (VALIDATION_MODE_ENABLED)
        {
            // Determine the optimal validation layers to enable that are necessary for useful debugging
            static const std::vector<const char*> validationLayerPriorityList[] =
            {
                // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
                {"VK_LAYER_KHRONOS_validation"},

                // Otherwise we fallback to using the LunarG meta layer
                {"VK_LAYER_LUNARG_standard_validation"},

                // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
                {
                    "VK_LAYER_GOOGLE_threading",
                    "VK_LAYER_LUNARG_parameter_validation",
                    "VK_LAYER_LUNARG_object_tracker",
                    "VK_LAYER_LUNARG_core_validation",
                    "VK_LAYER_GOOGLE_unique_objects",
                },

                // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
                {"VK_LAYER_LUNARG_core_validation"}
            };

            for (auto& validationLayers : validationLayerPriorityList)
            {
                if (ValidateLayers(validationLayers, availableInstanceLayers))
                {
                    for (auto& x : validationLayers)
                    {
                        instanceLayers.push_back(x);
                    }
                    break;
                }
            }
        }

        // Fill out application info
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "CybEngine Application";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "CybEngine";
        app_info.apiVersion = VK_API_VERSION_1_2;

        // Create instance:
        {
            VkInstanceCreateInfo instance_info = {};
            instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pApplicationInfo = &app_info;
            instance_info.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
            instance_info.ppEnabledLayerNames = instanceLayers.data();
            instance_info.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
            instance_info.ppEnabledExtensionNames = instanceExtensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

            if (VALIDATION_MODE_ENABLED && debugUtils)
            {
                debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
                instance_info.pNext = &debugUtilsCreateInfo;
                CYB_WARNING("Vulkan is running with validation layers enabled. This will heavily impact performace.");
            }

            res = vkCreateInstance(&instance_info, nullptr, &instance);
            if (res != VK_SUCCESS)
            {
                platform::CreateMessageWindow("vkCreateInstance failed! Error: " + std::to_string(res), "Error!");
                platform::Exit(1);
            }

            volkLoadInstanceOnly(instance);

            if (VALIDATION_MODE_ENABLED && debugUtils)
            {
                vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &debugUtilsMessenger);
            }
        }

        // Enumerate and create device
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            if (deviceCount == 0)
            {
                platform::CreateMessageWindow("Failed to find GPU with Vulkan support!", "Error!");
                platform::Exit(1);
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
            
            const std::vector<const char*> requiredDeviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            std::vector<const char*> enabledDeviceExtensions;

            for (const auto& dev : devices)
            {
                bool suitable = true;

                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
                std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableDeviceExtensions.data());

                for (auto& requiredExtension : requiredDeviceExtensions)
                {
                    if (!CheckExtensionSupport(requiredExtension, availableDeviceExtensions))
                    {
                        suitable = false;
                    }
                }
                if (!suitable)
                    continue;
                enabledDeviceExtensions = requiredDeviceExtensions;
                
                properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
                properties_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
                driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
                properties2.pNext = &properties_1_1;
                properties_1_1.pNext = &properties_1_2;
                properties_1_2.pNext = &driver_properties;
                vkGetPhysicalDeviceProperties2(dev, &properties2);

                bool discreteGPU = properties2.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
                if (discreteGPU || physicalDevice == VK_NULL_HANDLE)
                {
                    physicalDevice = dev;
                    if (discreteGPU)
                    {
                        // if this is discrete GPU, look no further (prioritize discrete GPU)
                        break;
                    }
                }
            }

            if (physicalDevice == VK_NULL_HANDLE)
            {
                platform::CreateMessageWindow("Failed to find a suitable GPU!");
                platform::Exit(1);
            }

            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            features2.pNext = &features_1_1;
            features_1_1.pNext = &features_1_2;

            vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
            assert(features2.features.geometryShader == VK_TRUE);
            assert(features2.features.samplerAnisotropy == VK_TRUE);

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

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::unordered_set<uint32_t> uniqueQueueFamilies = { graphicsFamily, copyFamily, computeFamily };

            float queue_priority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies)
            {
                VkDeviceQueueCreateInfo queue_info = {};
                queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_info.queueFamilyIndex = queueFamily;
                queue_info.queueCount = 1;
                queue_info.pQueuePriorities = &queue_priority;
                queueCreateInfos.push_back(queue_info);
                families.push_back(queueFamily);
            }

            VkDeviceCreateInfo device_info = {};
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
            device_info.pQueueCreateInfos = queueCreateInfos.data();
            device_info.pEnabledFeatures = nullptr;
            device_info.pNext = &features2;
            device_info.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
            device_info.ppEnabledExtensionNames = enabledDeviceExtensions.data();

            res = vkCreateDevice(physicalDevice, &device_info, nullptr, &device);
            if (res != VK_SUCCESS)
            {
                platform::CreateMessageWindow("vkCreateDevice failed! Error: " + std::to_string(res), "Error!");
                platform::Exit(1);
            }

            volkLoadDevice(device);
        }

        // Queues:
        {
            vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
            vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
            vkGetDeviceQueue(device, copyFamily, 0, &copyQueue);

            queues[static_cast<uint32_t>(QueueType::Graphics)].queue = graphicsQueue;
            queues[static_cast<uint32_t>(QueueType::Compute)].queue = computeQueue;
            queues[static_cast<uint32_t>(QueueType::Copy)].queue = copyQueue;
        }

        memory_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memory_properties_2);

        allocationhandler = std::make_shared<AllocationHandler>();
        allocationhandler->device = device;
        allocationhandler->instance = instance;

        // Initialize Vulkan Memory Allocator helper:
        VmaVulkanFunctions vma_vulkan_func{};
        vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vma_vulkan_func.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vma_vulkan_func.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vma_vulkan_func.vkAllocateMemory = vkAllocateMemory;
        vma_vulkan_func.vkFreeMemory = vkFreeMemory;
        vma_vulkan_func.vkMapMemory = vkMapMemory;
        vma_vulkan_func.vkUnmapMemory = vkUnmapMemory;
        vma_vulkan_func.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vma_vulkan_func.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vma_vulkan_func.vkBindBufferMemory = vkBindBufferMemory;
        vma_vulkan_func.vkBindImageMemory = vkBindImageMemory;
        vma_vulkan_func.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vma_vulkan_func.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vma_vulkan_func.vkCreateBuffer = vkCreateBuffer;
        vma_vulkan_func.vkDestroyBuffer = vkDestroyBuffer;
        vma_vulkan_func.vkCreateImage = vkCreateImage;
        vma_vulkan_func.vkDestroyImage = vkDestroyImage;
        vma_vulkan_func.vkCmdCopyBuffer = vkCmdCopyBuffer;

        VmaAllocatorCreateInfo alloc_info = {};
        alloc_info.physicalDevice = physicalDevice;
        alloc_info.device = device;
        alloc_info.instance = instance;
        alloc_info.pVulkanFunctions = &vma_vulkan_func;

        res = vmaCreateAllocator(&alloc_info, &allocationhandler->allocator);
        if (res != VK_SUCCESS)
        {
            platform::CreateMessageWindow("vmaCreateAllocator failed! ERROR: " + std::to_string(res), "Error!");
            platform::Exit();
        }

        copy_allocator.Init(this);

        // Create frame resources:
        {
            for (uint32_t i = 0; i < BUFFERCOUNT; ++i)
            {
                for (uint32_t q = 0; q < static_cast<uint32_t>(QueueType::_Count); ++q)
                {
                    VkFenceCreateInfo fenceInfo = {};
                    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                    //fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                    VkResult res = vkCreateFence(device, &fenceInfo, nullptr, &frame_resources[i].fence[q]);
                    assert(res == VK_SUCCESS);
                }

                // Create resources for transition command buffer:
                VkCommandPoolCreateInfo pool_info = {};
                pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                pool_info.queueFamilyIndex = graphicsFamily;
                pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
                res = vkCreateCommandPool(device, &pool_info, nullptr, &frame_resources[i].init_commandpool);
                assert(res == VK_SUCCESS);

                VkCommandBufferAllocateInfo commandbuffer_info = {};
                commandbuffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                commandbuffer_info.commandBufferCount = 1;
                commandbuffer_info.commandPool = frame_resources[i].init_commandpool;
                commandbuffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                res = vkAllocateCommandBuffers(device, &commandbuffer_info, &frame_resources[i].init_commandbuffer);
                assert(res == VK_SUCCESS);

                VkCommandBufferBeginInfo begin_info = {};
                begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                begin_info.pInheritanceInfo = nullptr; // Optional

                res = vkBeginCommandBuffer(frame_resources[i].init_commandbuffer, &begin_info);
                assert(res == VK_SUCCESS);
            }
        }

        timestampFrequency = uint64_t(1.0 / double(properties2.properties.limits.timestampPeriod) * 1000 * 1000 * 1000);

        // Dynamic PSO states:
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        pso_dynamic_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount = (uint32_t)pso_dynamic_states.size();
        dynamic_state_info.pDynamicStates = pso_dynamic_states.data();

        // Create pipeline cache
        // TODO: Load pipeline cache from disk
        {
            VkPipelineCacheCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
            //createInfo.initialDataSize = pipelineData.size();
            //createInfo.pInitialData = pipelineData.data();

            // Create Vulkan pipeline cache
            res = vkCreatePipelineCache(device, &createInfo, nullptr, &pipeline_cache);
            assert(res == VK_SUCCESS);
        }

        CYB_INFO("Initialized Vulkan {0}.{1}", VK_API_VERSION_MAJOR(properties2.properties.apiVersion), VK_API_VERSION_MINOR(properties2.properties.apiVersion));
        CYB_INFO("Using {0}", properties2.properties.deviceName);
        CYB_INFO("Driver {0} {1}", driver_properties.driverName, driver_properties.driverInfo);;
    }
    
    GraphicsDevice_Vulkan::~GraphicsDevice_Vulkan()
    {
        VkResult res = vkDeviceWaitIdle(device);
        assert(res == VK_SUCCESS);

        for (auto& x : pipelines_global)
        {
            vkDestroyPipeline(device, x.second, nullptr);
        }

        if (debugUtilsMessenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);
        }

        for(auto& frame : frame_resources)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(QueueType::_Count); ++i)
            {
                vkDestroyFence(device, frame.fence[i], nullptr);
            }
            
            vkDestroyCommandPool(device, frame.init_commandpool, nullptr);
        }

        copy_allocator.Destroy();

        for (auto& x : pso_layout_cache)
        {
            vkDestroyPipelineLayout(device, x.second.pipeline_layout, nullptr);
            vkDestroyDescriptorSetLayout(device, x.second.descriptorset_layout, nullptr);
        }

        if (pipeline_cache != VK_NULL_HANDLE)
        {
            // TODO: Save pipeline cache to disk
            vkDestroyPipelineCache(device, pipeline_cache, nullptr);
            pipeline_cache = VK_NULL_HANDLE;
        }

        for (auto& commandlist : commandlists)
        {
            for (uint32_t buffer_index = 0; buffer_index < BUFFERCOUNT; ++buffer_index)
            {
                for (uint32_t q = 0; q < static_cast<uint32_t>(QueueType::_Count); ++q)
                {
                    vkDestroyCommandPool(device, commandlist->commandpools[buffer_index][q], nullptr);
                }
            }

            for (auto& x : commandlist->binder_pools)
            {
                x.Destroy();
            }
        }
    }

    bool GraphicsDevice_Vulkan::CreateSwapChain(const SwapChainDesc* desc, platform::Window* window, SwapChain* swapchain) const
    {
        auto internal_state = std::static_pointer_cast<SwapChain_Vulkan>(swapchain->internal_state);
        if (swapchain->internal_state == nullptr)
        {
            internal_state = std::make_shared<SwapChain_Vulkan>();
        }
        internal_state->allocationhandler = allocationhandler;
        internal_state->desc = *desc;
        swapchain->internal_state = internal_state;
        swapchain->desc = *desc;

        // Surface creation:
        if (internal_state->surface == VK_NULL_HANDLE)
        {
#ifdef _WIN32
            VkWin32SurfaceCreateInfoKHR create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            create_info.hwnd = (HWND)window->GetNativePtr();
            create_info.hinstance = (HINSTANCE)platform::GetInstance();
            vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &internal_state->surface);
#else
#error VULKAN DEVICE ERROR: PLATFORM NOT SUPPORTED
#endif // _WIN32
        }

        uint32_t present_family = VK_QUEUE_FAMILY_IGNORED;
        for (size_t family_index = 0; family_index < queueFamilies.size(); ++family_index)
        {
            VkBool32 supported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, (uint32_t)family_index, internal_state->surface, &supported);

            if (present_family == VK_QUEUE_FAMILY_IGNORED && queueFamilies[family_index].queueCount > 0 && supported)
            {
                present_family = (uint32_t)family_index;
                break;
            }
        }

        // Present family not found, we cannot create SwapChain
        if (present_family == VK_QUEUE_FAMILY_IGNORED)
        {
            return false;
        }

        return CreateSwapChain_Internal(internal_state.get(), physicalDevice, device, allocationhandler);
    }

    bool GraphicsDevice_Vulkan::CreateBuffer(const GPUBufferDesc* desc, const void* init_data, GPUBuffer* buffer) const
    {
        auto internal_state = std::make_shared<Buffer_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
        buffer->internal_state = internal_state;
        buffer->type = GPUResource::Type::Buffer;
        buffer->mappedData = nullptr;
        buffer->mappedRowPitch = 0;
        buffer->desc = *desc;

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buffer->desc.size;
        buffer_info.usage = 0;

        if (HasFlag(buffer->desc.bindFlags, BindFlags::VertexBufferBit))
            buffer_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (HasFlag(buffer->desc.bindFlags, BindFlags::IndexBufferBit))
            buffer_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (HasFlag(buffer->desc.bindFlags, BindFlags::ConstantBufferBit))
            buffer_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (HasFlag(buffer->desc.miscFlags, ResourceMiscFlag::BufferRawBit))
            buffer_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (HasFlag(buffer->desc.miscFlags, ResourceMiscFlag::BufferStructuredBit))
            buffer_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.flags = 0;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        if (desc->usage == MemoryAccess::Readback) 
        {
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else if (desc->usage == MemoryAccess::Upload)
        {
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
       
        VkResult res = vmaCreateBuffer(allocationhandler->allocator, &buffer_info, &alloc_info, &internal_state->resource, &internal_state->allocation, nullptr);
        assert(res == VK_SUCCESS);

        if (desc->usage == MemoryAccess::Readback || desc->usage == MemoryAccess::Upload)
        {
            buffer->mappedData = internal_state->allocation->GetMappedData();
            buffer->mappedRowPitch = static_cast<uint32_t>(desc->size);
        }

        // Issue data copy on request:
        if (init_data != nullptr)
        {
            auto cmd = copy_allocator.Allocate(desc->size);

            std::memcpy(cmd.uploadbuffer.mappedData, init_data, buffer->desc.size);

            VkBufferMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.buffer = internal_state->resource;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.size = VK_WHOLE_SIZE;

            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            vkCmdPipelineBarrier(
                cmd.commandbuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr
            );

            VkBufferCopy copyRegion = {};
            copyRegion.size = buffer->desc.size;
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;

            vkCmdCopyBuffer(
                cmd.commandbuffer,
                ToInternal(&cmd.uploadbuffer)->resource,
                internal_state->resource,
                1,
                &copyRegion
            );

            std::swap(barrier.srcAccessMask, barrier.dstAccessMask);

            if (HasFlag(buffer->desc.bindFlags, BindFlags::ConstantBufferBit))
                barrier.dstAccessMask |= VK_ACCESS_UNIFORM_READ_BIT;
            if (HasFlag(buffer->desc.bindFlags, BindFlags::VertexBufferBit))
                barrier.dstAccessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            if (HasFlag(buffer->desc.bindFlags, BindFlags::IndexBufferBit))
                barrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;

            vkCmdPipelineBarrier(
                cmd.commandbuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr
            );

            copy_allocator.Submit(cmd);
        }

        return R_SUCCESS;
    }

    bool GraphicsDevice_Vulkan::CreateQuery(const GPUQueryDesc* desc, GPUQuery* query) const
    {
        auto internal_state = std::make_shared<Query_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
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

        VkResult res = vkCreateQueryPool(device, &poolInfo, nullptr, &internal_state->pool);
        assert(res == VK_SUCCESS);

        return (res == VK_SUCCESS);
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
            hash::HashCombine(hash, strides[i]);
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
        if (index_buffer != nullptr)
        {
            auto internal_state = ToInternal(index_buffer);
            CommandList_Vulkan& commandlist = GetCommandList(cmd);
            vkCmdBindIndexBuffer(commandlist.GetCommandBuffer(), internal_state->resource, offset, format == IndexBufferFormat::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        }
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
            binder.dirty |= DescriptorBinder::DIRTY_DESCRIPTOR;
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
            binder.dirty |= DescriptorBinder::DIRTY_DESCRIPTOR;
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
            binder.dirty |= DescriptorBinder::DIRTY_DESCRIPTOR;
        }

        if (binder.table.CBV_offset[slot] != offset)
        {
            binder.table.CBV_offset[slot] = offset;
            binder.dirty |= DescriptorBinder::DIRTY_DESCRIPTOR;
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

    void GraphicsDevice_Vulkan::CreateSubresource(Texture* texture, SubresourceType type) const
    {
        auto internal_state = ToInternal(texture);

        Format format = texture->GetDesc().format;

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = internal_state->resource;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = _ConvertFormat(format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        const TextureComponentMapping& swizzle = texture->GetDesc().components;
        viewInfo.components.r = _ConvertComponentSwizzle(swizzle.r);
        viewInfo.components.g = _ConvertComponentSwizzle(swizzle.g);
        viewInfo.components.b = _ConvertComponentSwizzle(swizzle.b);
        viewInfo.components.a = _ConvertComponentSwizzle(swizzle.a);

        switch (type)
        {
        case SubresourceType::SRV:
        {
            switch (format)
            {
            case Format::D32_Float_S8_Uint:
                viewInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                break;
            }

            Texture_Vulkan::TextureSubresource subresource;
            VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &subresource.image_view);
            assert(res == VK_SUCCESS);

            assert(internal_state->srv.image_view == VK_NULL_HANDLE);
            internal_state->srv = subresource;
        } break;
        case SubresourceType::RTV:
        {
            assert(internal_state->rtv == VK_NULL_HANDLE);
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &internal_state->rtv);
            assert(res == VK_SUCCESS);
        } break;
        case SubresourceType::DSV:
        {
            assert(internal_state->dsv == VK_NULL_HANDLE);
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &internal_state->dsv);
            assert(res == VK_SUCCESS);
        } break;

        default:
            assert(0);
            break;
        }
    }

    bool GraphicsDevice_Vulkan::CreateTexture(const TextureDesc* desc, const SubresourceData* init_data, Texture* texture) const
    {
        assert(texture != nullptr);
        assert(desc->format != Format::Unknown);
        auto internal_state = std::make_shared<Texture_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
        texture->internal_state = internal_state;
        texture->type = GPUResource::Type::Texture;
        texture->desc = *desc;

        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.format = _ConvertFormat(texture->desc.format);
        image_info.extent.width = texture->desc.width;
        image_info.extent.height = texture->desc.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.usage = 0;

        if (HasFlag(texture->desc.bindFlags, BindFlags::ShaderResourceBit))
            image_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (HasFlag(texture->desc.bindFlags, BindFlags::RenderTargetBit))
            image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (HasFlag(texture->desc.bindFlags, BindFlags::DepthStencilBit))
            image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        switch (texture->desc.type)
        {
        case TextureDesc::Type::Texture1D:
            image_info.imageType = VK_IMAGE_TYPE_1D;
            break;
        case TextureDesc::Type::Texture2D:
            image_info.imageType = VK_IMAGE_TYPE_2D;
            break;
        case TextureDesc::Type::Texture3D:
            image_info.imageType = VK_IMAGE_TYPE_3D;
            break;
        default:
            assert(0);
            break;
        }

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

        VkResult res = vmaCreateImage(
            allocationhandler->allocator,
            &image_info,
            &alloc_info,
            &internal_state->resource,
            &internal_state->allocation,
            nullptr);
        assert(res == VK_SUCCESS);

        if (init_data != nullptr)
        {
            auto cmd = copy_allocator.Allocate(internal_state->allocation->GetSize());

            std::vector<VkBufferImageCopy> copy_regions;

            VkDeviceSize copy_offset = 0;
            uint32_t init_data_index = 0;
            for (uint32_t layer = 0; layer < desc->arraySize; ++layer)
            {
                uint32_t width = image_info.extent.width;
                uint32_t height = image_info.extent.height;
                uint32_t depth = image_info.extent.depth;
                for (uint32_t mip = 0; mip < desc->mipLevels; ++mip)
                {
                    const SubresourceData& subresourceData = init_data[init_data_index++];
                    assert(subresourceData.mem != nullptr);
                    const uint32_t block_size = 1; // GetFormatBlockSize(desc->format);
                    const uint32_t num_blocks_x = width / block_size;
                    const uint32_t num_blocks_y = height / block_size;
                    const uint32_t dst_rowpitch = num_blocks_x * GetFormatStride(desc->format);
                    const uint32_t dst_slicepitch = dst_rowpitch * num_blocks_y;
                    const uint32_t src_rowpitch = subresourceData.rowPitch;
                    const uint32_t src_slicepitch = subresourceData.slicePitch;
                    for (uint32_t z = 0; z < depth; ++z)
                    {
                        uint8_t* dst_slice = (uint8_t*)cmd.uploadbuffer.mappedData + copy_offset + dst_slicepitch * z;
                        uint8_t* src_slice = (uint8_t*)subresourceData.mem + src_slicepitch * z;
                        for (uint32_t y = 0; y < num_blocks_y; ++y)
                        {
                            std::memcpy(
                                dst_slice + dst_rowpitch * y,
                                src_slice + src_rowpitch * y,
                                dst_rowpitch
                            );
                        }
                    }

                    VkBufferImageCopy copyRegion = {};
                    copyRegion.bufferOffset = copy_offset;
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

                    copy_regions.push_back(copyRegion);
                    copy_offset += dst_slicepitch * depth;

                    width = std::max(1u, width / 2);
                    height = std::max(1u, height / 2);
                    depth = std::max(1u, depth / 2);
                }
            }

            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.image = internal_state->resource;
                barrier.oldLayout = image_info.initialLayout;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = image_info.arrayLayers;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = image_info.mipLevels;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                vkCmdPipelineBarrier(
                    cmd.commandbuffer,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                vkCmdCopyBufferToImage(
                    cmd.commandbuffer,
                    ToInternal(&cmd.uploadbuffer)->resource,
                    internal_state->resource,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    (uint32_t)copy_regions.size(),
                    copy_regions.data());

                copy_allocator.Submit(cmd);

                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = _ConvertImageLayout(texture->desc.layout);
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = _ParseResourceState(texture->desc.layout);

                init_locker.lock();
                vkCmdPipelineBarrier(
                    GetFrameResources().init_commandbuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );
                init_submits = true;
                init_locker.unlock();
            }
        }
        else
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = internal_state->resource;
            barrier.oldLayout = image_info.initialLayout;
            barrier.newLayout = _ConvertImageLayout(texture->desc.layout);
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = _ParseResourceState(texture->desc.layout);
            if (HasFlag(texture->desc.bindFlags, BindFlags::DepthStencilBit))
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            else
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = image_info.arrayLayers;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = image_info.mipLevels;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            init_locker.lock();
            vkCmdPipelineBarrier(
                GetFrameResources().init_commandbuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
            init_submits = true;
            init_locker.unlock();
        }

        if (HasFlag(texture->desc.bindFlags, BindFlags::ShaderResourceBit))
            CreateSubresource(texture, SubresourceType::SRV);
        if (HasFlag(texture->desc.bindFlags, BindFlags::RenderTargetBit))
            CreateSubresource(texture, SubresourceType::RTV);
        if (HasFlag(texture->desc.bindFlags, BindFlags::DepthStencilBit))
            CreateSubresource(texture, SubresourceType::DSV);

        return R_SUCCESS;
    }

    GraphicsDevice::MemoryUsage GraphicsDevice_Vulkan::GetMemoryUsage() const
    {
        MemoryUsage result = {};
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
        vmaGetHeapBudgets(allocationhandler->allocator, budgets);
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
        if (HasFlag(desc->bindFlags, BindFlags::ConstantBufferBit))
            alignment = std::max(alignment, properties2.properties.limits.minUniformBufferOffsetAlignment);
        else
            alignment = std::max(alignment, properties2.properties.limits.minTexelBufferOffsetAlignment);
        return alignment;
    }

    bool GraphicsDevice_Vulkan::CreateShader(ShaderStage stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const
    {
        assert(shader != nullptr);
        assert(shaderBytecode != nullptr);
        assert(bytecodeLength > 0);

        auto internal_state = std::make_shared<Shader_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
        shader->internal_state = internal_state;
        shader->stage = stage;
  
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = bytecodeLength;
        create_info.pCode = (const uint32_t*)shaderBytecode;
        VkResult res = vkCreateShaderModule(device, &create_info, nullptr, &internal_state->shadermodule);
        assert(res == VK_SUCCESS);

        internal_state->stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        internal_state->stage_info.module = internal_state->shadermodule;
        internal_state->stage_info.pName = "main";
        switch (stage)
        {
        case ShaderStage::VS:
            internal_state->stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderStage::GS:
            internal_state->stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ShaderStage::FS:
            internal_state->stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        default:
            // also means library shader (ray tracing)
            internal_state->stage_info.stage = VK_SHADER_STAGE_ALL;
            break;
        }

        {
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(create_info.codeSize, create_info.pCode, &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            uint32_t binding_count = 0;
            result = spvReflectEnumerateDescriptorBindings(
                &module, &binding_count, nullptr
            );
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
            result = spvReflectEnumerateDescriptorBindings(
                &module, &binding_count, bindings.data()
            );
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            for (auto& x : bindings)
            {
                const bool bindless = x->set > 0;
                assert(bindless == false);   // NO SUPPORT FOR BINDLESS ATM
                
                auto& descriptor = internal_state->layout_bindings.emplace_back();
                descriptor.stageFlags = internal_state->stage_info.stage;
                descriptor.binding = x->binding;
                descriptor.descriptorCount = x->count;
                descriptor.descriptorType = (VkDescriptorType)x->descriptor_type;

                auto& imageview_type = internal_state->imageview_types.emplace_back();
                imageview_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

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
            }

            spvReflectDestroyShaderModule(&module);
        }

        return R_SUCCESS;
    }

    bool GraphicsDevice_Vulkan::CreateSampler(const SamplerDesc* desc, Sampler* sampler) const
    {
        auto internal_state = std::make_shared<Sampler_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
        sampler->internal_state = internal_state;
        sampler->desc = *desc;

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.flags = 0;
        sampler_info.pNext = nullptr;

        switch (desc->filter)
        {
        case TextureFilter::Point:
            sampler_info.minFilter = VK_FILTER_NEAREST;
            sampler_info.magFilter = VK_FILTER_NEAREST;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            break;
        case TextureFilter::Bilinear:
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            break;
        case TextureFilter::Trilinear:
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.compareEnable = VK_FALSE;
            break;
        case TextureFilter::Anisotropic:
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.anisotropyEnable = VK_TRUE;
            sampler_info.compareEnable = VK_FALSE;
            break;
        }

        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.maxAnisotropy = desc->maxAnisotropy;
        sampler_info.mipLodBias = desc->lodBias;
        sampler_info.minLod = desc->minLOD;
        sampler_info.maxLod = desc->maxLOD;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        VkResult res = vkCreateSampler(device, &sampler_info, nullptr, &internal_state->resource);
        assert(res == VK_SUCCESS);

        return R_SUCCESS;
    }

    bool GraphicsDevice_Vulkan::CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const
    {
        auto internal_state = std::make_shared<PipelineState_Vulkan>();
        pso->internal_state = internal_state;
        pso->desc = *desc;

        pso->hash = 0;
        hash::HashCombine(pso->hash, desc->vs);
        hash::HashCombine(pso->hash, desc->gs);
        hash::HashCombine(pso->hash, desc->fs);
        hash::HashCombine(pso->hash, desc->rs);
        hash::HashCombine(pso->hash, desc->dss);
        hash::HashCombine(pso->hash, desc->il);
        hash::HashCombine(pso->hash, desc->pt);

        // Create bindings:
        {
            auto insert_shader = [&](const Shader* shader) {
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
                        internal_state->imageview_types.push_back(shader_internal->imageview_types[i]);
                    
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
            };

            insert_shader(desc->vs);
            insert_shader(desc->gs);
            insert_shader(desc->fs);

            // sort because dynamic offsets array is tightly packed to match slot numbers:
            std::sort(internal_state->uniform_buffer_dynamic_slots.begin(), internal_state->uniform_buffer_dynamic_slots.end());
        }

        internal_state->binding_hash = 0;
        uint32_t i = 0;
        for (auto& x : internal_state->layout_bindings)
        {
            hash::HashCombine(internal_state->binding_hash, x.binding);
            hash::HashCombine(internal_state->binding_hash, x.descriptorCount);
            hash::HashCombine(internal_state->binding_hash, x.descriptorType);
            hash::HashCombine(internal_state->binding_hash, x.stageFlags);
            hash::HashCombine(internal_state->binding_hash, internal_state->imageview_types[i++]);
        }

        pso_layout_cache_mutex.lock();
        if (pso_layout_cache[internal_state->binding_hash].pipeline_layout == VK_NULL_HANDLE)
        {
            VkDescriptorSetLayoutCreateInfo descriptorset_layout_info = {};
            descriptorset_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorset_layout_info.bindingCount = static_cast<uint32_t>(internal_state->layout_bindings.size());
            descriptorset_layout_info.pBindings = internal_state->layout_bindings.data();
            VkResult res = vkCreateDescriptorSetLayout(device, &descriptorset_layout_info, nullptr, &internal_state->descriptorset_layout);
            assert(res == VK_SUCCESS);

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &internal_state->descriptorset_layout;
            res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &internal_state->pipelineLayout);
            assert(res == VK_SUCCESS);

            pso_layout_cache[internal_state->binding_hash].descriptorset_layout = internal_state->descriptorset_layout;
            pso_layout_cache[internal_state->binding_hash].pipeline_layout = internal_state->pipelineLayout;
        }
        else
        {
            internal_state->descriptorset_layout = pso_layout_cache[internal_state->binding_hash].descriptorset_layout;
            internal_state->pipelineLayout = pso_layout_cache[internal_state->binding_hash].pipeline_layout;
        }
        pso_layout_cache_mutex.unlock();

        // Viewport & Scissors:
        VkViewport& viewport = internal_state->viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 65535;
        viewport.height = 65535;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D& scissor = internal_state->scissor;
        scissor.extent.width = 65535;
        scissor.extent.height = 65535;

        VkPipelineViewportStateCreateInfo& viewportState = internal_state->viewportState;
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // Depth-Stencil:
        VkPipelineDepthStencilStateCreateInfo& depthstencil = internal_state->depthstencil;
        depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (pso->desc.dss != nullptr)
        {
            depthstencil.depthTestEnable = pso->desc.dss->depthEnable ? VK_TRUE : VK_FALSE;
            depthstencil.depthWriteEnable = pso->desc.dss->depthWriteMask == DepthWriteMask::Zero ? VK_FALSE : VK_TRUE;
            depthstencil.depthCompareOp = _ConvertComparisonFunc(pso->desc.dss->depthFunc);

            depthstencil.stencilTestEnable = pso->desc.dss->stencilEnable ? VK_TRUE : VK_FALSE;

            depthstencil.front.compareMask = pso->desc.dss->stencilReadMask;
            depthstencil.front.writeMask = pso->desc.dss->stencilWriteMask;
            depthstencil.front.reference = 0; // runtime supplied
            depthstencil.front.compareOp = _ConvertComparisonFunc(pso->desc.dss->frontFace.stencilFunc);
            depthstencil.front.passOp = _ConvertStencilOp(pso->desc.dss->frontFace.stencilPassOp);
            depthstencil.front.failOp = _ConvertStencilOp(pso->desc.dss->frontFace.stencilFailOp);
            depthstencil.front.depthFailOp = _ConvertStencilOp(pso->desc.dss->frontFace.stencilDepthFailOp);

            depthstencil.back.compareMask = pso->desc.dss->stencilReadMask;
            depthstencil.back.writeMask = pso->desc.dss->stencilWriteMask;
            depthstencil.back.reference = 0; // runtime supplied
            depthstencil.back.compareOp = _ConvertComparisonFunc(pso->desc.dss->backFace.stencilFunc);
            depthstencil.back.passOp = _ConvertStencilOp(pso->desc.dss->backFace.stencilPassOp);
            depthstencil.back.failOp = _ConvertStencilOp(pso->desc.dss->backFace.stencilFailOp);
            depthstencil.back.depthFailOp = _ConvertStencilOp(pso->desc.dss->backFace.stencilDepthFailOp);

            if (0 /*CheckCapability(GraphicsDeviceCapability::DEPTH_BOUNDS_TEST)*/)
            {
                depthstencil.depthBoundsTestEnable = pso->desc.dss->depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
            }
            else
            {
                depthstencil.depthBoundsTestEnable = VK_FALSE;
            }
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

            switch (rs.fillMode)
            {
            case FillMode::Whireframe:
                rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
                break;
            case FillMode::Solid:
            default:
                rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
                break;
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
        auto validate_and_add_shadder = [&](const Shader* shader)
        {
            if (shader != nullptr && shader->IsValid())
            {
                auto shaderInternal = ToInternal(shader);
                internal_state->shaderStages[shaderStageCount++] = ToInternal(shader)->stage_info;
            }
        };

        validate_and_add_shadder(pso->desc.vs);
        validate_and_add_shadder(pso->desc.gs);
        validate_and_add_shadder(pso->desc.fs);
        if (shaderStageCount == 0)
        {
            CYB_ERROR("Pipeline has no valid shader attached!");
            return R_FAIL;
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

        return R_SUCCESS;
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
        vkCmdSetScissor(commandlist.GetCommandBuffer(), 0, rectCount, scissors);
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

        size_t pipeline_hash = 0;
        hash::HashCombine(pipeline_hash, pso->hash);
        if (commandlist.active_renderpass != nullptr)
            hash::HashCombine(pipeline_hash, commandlist.active_renderpass->hash);
        if (pipeline_hash == commandlist.prev_pipeline_hash)
            return;
        commandlist.prev_pipeline_hash = pipeline_hash;

        auto internal_state = ToInternal(pso);

        if (commandlist.active_pso == nullptr)
        {
            commandlist.binder.dirty |= DescriptorBinder::DIRTY_ALL;
        }
        else
        {
            auto active_internal = ToInternal(commandlist.active_pso);
            if (internal_state->binding_hash != active_internal->binding_hash)
                commandlist.binder.dirty |= DescriptorBinder::DIRTY_ALL;
        }

        commandlist.active_pso = pso;
        commandlist.dirty_pso = true;
    }

    bool GraphicsDevice_Vulkan::CreateRenderPass(const RenderPassDesc* desc, RenderPass* renderpass) const
    {
        auto internal_state = std::make_shared<RenderPass_Vulkan>();
        internal_state->allocationhandler = allocationhandler;
        renderpass->internal_state = internal_state;
        renderpass->desc = *desc;

        renderpass->hash = 0;
        hash::HashCombine(renderpass->hash, desc->attachments.size());
        for (const auto& attachment : desc->attachments)
        {
            if (attachment.type == RenderPassAttachment::Type::RenderTarget ||
                attachment.type == RenderPassAttachment::Type::DepthStencil)
            {
                hash::HashCombine(renderpass->hash, attachment.texture->desc.format);
            }
        }

        VkImageView attachments[8] = {};
        VkAttachmentDescription attachmentDescriptions[8] = {};
        VkAttachmentReference colorAttachmentRefs[6] = {};
        VkAttachmentReference depthAttachmentRef = {};

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        uint32_t validAttachmentCount = 0;
        for (auto& attachment : renderpass->desc.attachments)
        {
            const Texture* texture = attachment.texture;
            const TextureDesc& texdesc = texture->desc;
            auto textureInternalState = ToInternal(texture);

            VkAttachmentDescription& attachmentDesc = attachmentDescriptions[validAttachmentCount];
            attachmentDesc.format = _ConvertFormat(texdesc.format);
            attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDesc.loadOp = _ConvertLoadOp(attachment.loadOp);
            attachmentDesc.storeOp = _ConvertStoreOp(attachment.storeOp);
            attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDesc.initialLayout = _ConvertImageLayout(attachment.initialLayout);
            attachmentDesc.finalLayout = _ConvertImageLayout(attachment.finalLayout);

            switch (attachment.type)
            {
            case RenderPassAttachment::Type::RenderTarget:
            {
                attachments[validAttachmentCount] = textureInternalState->rtv;
                colorAttachmentRefs[subpass.colorAttachmentCount].attachment = validAttachmentCount;
                colorAttachmentRefs[subpass.colorAttachmentCount].layout = _ConvertImageLayout(attachment.subpassLayout);
                subpass.colorAttachmentCount++;
                subpass.pColorAttachments = colorAttachmentRefs;
            } break;
            case RenderPassAttachment::Type::DepthStencil:
            {
                attachments[validAttachmentCount] = textureInternalState->dsv;
                depthAttachmentRef.attachment = validAttachmentCount;
                depthAttachmentRef.layout = _ConvertImageLayout(attachment.subpassLayout);
                subpass.pDepthStencilAttachment = &depthAttachmentRef;
            } break;
            default: assert(0);
            }

            if (attachments[validAttachmentCount] == VK_NULL_HANDLE)
            {
                continue;
            }

            validAttachmentCount++;
        }
        assert(renderpass->desc.attachments.size() == validAttachmentCount);

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = validAttachmentCount;
        renderPassInfo.pAttachments = attachmentDescriptions;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        vkCreateRenderPass(device, &renderPassInfo, nullptr, &internal_state->renderpass);
        
        // Create framebuffer:
        const TextureDesc& texdesc = renderpass->desc.attachments[0].texture->desc;
        auto textureInternal = ToInternal(renderpass->desc.attachments[0].texture);

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = internal_state->renderpass;
        framebufferInfo.attachmentCount = validAttachmentCount;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = texdesc.width;
        framebufferInfo.height = texdesc.height;
        framebufferInfo.layers = 1;

        VkResult res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &internal_state->framebuffer);
        assert(res == VK_SUCCESS);

        // Setup beginInfo:
        internal_state->begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        internal_state->begin_info.renderPass = internal_state->renderpass;
        internal_state->begin_info.framebuffer = internal_state->framebuffer;
        internal_state->begin_info.renderArea.offset = { 0, 0 };
        internal_state->begin_info.renderArea.extent.width = framebufferInfo.width;
        internal_state->begin_info.renderArea.extent.height = framebufferInfo.height;

        internal_state->begin_info.clearValueCount = validAttachmentCount;
        internal_state->begin_info.pClearValues = internal_state->clear_values;

        int i = 0;
        for (auto& attachment : renderpass->desc.attachments)
        {
            if (renderpass->desc.attachments[i].type == RenderPassAttachment::Type::RenderTarget)
            {
                internal_state->clear_values[i].color.float32[0] = 0.0f;
                internal_state->clear_values[i].color.float32[1] = 0.0f;
                internal_state->clear_values[i].color.float32[2] = 0.0f;
                internal_state->clear_values[i].color.float32[3] = 0.0f;
            }
            else if (renderpass->desc.attachments[i].type == RenderPassAttachment::Type::DepthStencil)
            {
                internal_state->clear_values[i].depthStencil.depth = 0.0f;
                internal_state->clear_values[i].depthStencil.stencil = 0;
            }
            else
            {
                assert(0);
            }
            i++;
        }

        return R_SUCCESS;
    }

    CommandList GraphicsDevice_Vulkan::BeginCommandList(QueueType queue)
    {
        cmd_locker.lock();
        const uint32_t cmd_current = cmd_count++;
        if (cmd_current >= commandlists.size())
        {
            commandlists.push_back(std::make_unique<CommandList_Vulkan>());
        }
        CommandList cmd;
        cmd.internal_state = commandlists[cmd_current].get();
        cmd_locker.unlock();

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.Reset(GetBufferIndex());
        commandlist.queue = queue;
        commandlist.id = cmd_current;

        if (commandlist.GetCommandBuffer() == VK_NULL_HANDLE)
        {
            // Need to create one more command list:
            for (uint32_t buffer_index = 0; buffer_index < BUFFERCOUNT; ++buffer_index)
            {
                VkCommandPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
                poolInfo.queueFamilyIndex = graphicsFamily;
                VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandlist.commandpools[buffer_index][static_cast<uint32_t>(queue)]);
                assert(res == VK_SUCCESS);

                VkCommandBufferAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = commandlist.commandpools[buffer_index][static_cast<uint32_t>(queue)];
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                res = vkAllocateCommandBuffers(device, &allocInfo, &commandlist.commandbuffers[buffer_index][static_cast<uint32_t>(queue)]);
                assert(res == VK_SUCCESS);

                commandlist.binder_pools[buffer_index].Init(this);
            }

            commandlist.binder.Init(this);
        }

        VkResult res = vkResetCommandPool(device, commandlist.GetCommandPool(), 0);
        assert(res == VK_SUCCESS);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;
        res = vkBeginCommandBuffer(commandlist.GetCommandBuffer(), &beginInfo);
        assert(res == VK_SUCCESS);

        VkRect2D scissors[16];
        for (int i = 0; i < _countof(scissors); ++i)
        {
            scissors[i].offset.x = 0;
            scissors[i].offset.y = 0;
            scissors[i].extent.width = 65535;
            scissors[i].extent.height = 65535;
        }
        vkCmdSetScissor(commandlist.GetCommandBuffer(), 0, _countof(scissors), scissors);

        return cmd;
    }

    void GraphicsDevice_Vulkan::CommandQueue::Submit(GraphicsDevice_Vulkan* device, VkFence fence)
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = (uint32_t)submit_cmds.size();
        submitInfo.pCommandBuffers = submit_cmds.data();

        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(submit_waitSemaphores.size());
        submitInfo.pWaitSemaphores = submit_waitSemaphores.data();
        submitInfo.pWaitDstStageMask = submit_waitStages.data();

        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(submit_signalSemaphores.size());
        submitInfo.pSignalSemaphores = submit_signalSemaphores.data();

        VkTimelineSemaphoreSubmitInfo timelineInfo = {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = nullptr;
        timelineInfo.waitSemaphoreValueCount = (uint32_t)submit_waitValues.size();
        timelineInfo.pWaitSemaphoreValues = submit_waitValues.data();
        timelineInfo.signalSemaphoreValueCount = (uint32_t)submit_signalValues.size();
        timelineInfo.pSignalSemaphoreValues = submit_signalValues.data();

        submitInfo.pNext = &timelineInfo;

        VkResult res = vkQueueSubmit(queue, 1, &submitInfo, fence);
        assert(res == VK_SUCCESS);

        if (!submit_swapchains.empty())
        {
            VkPresentInfoKHR present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = static_cast<uint32_t>(submit_signalSemaphores.size());
            present_info.pWaitSemaphores = submit_signalSemaphores.data();

            present_info.swapchainCount = static_cast<uint32_t>(submit_swapchains.size());
            present_info.pSwapchains = submit_swapchains.data();
            present_info.pImageIndices = submit_swapChainImageIndices.data();

            res = vkQueuePresentKHR(queue, &present_info);
            assert(res == VK_SUCCESS);
        }

        submit_swapchains.clear();
        submit_swapChainImageIndices.clear();
        submit_waitStages.clear();
        submit_waitSemaphores.clear();
        submit_waitValues.clear();
        submit_signalSemaphores.clear();
        submit_signalValues.clear();
        submit_cmds.clear();
    }

    void GraphicsDevice_Vulkan::SubmitCommandList()
    {
        VkResult res;
        init_locker.lock();

        // Submit current frame:
        {
            auto& frame = GetFrameResources();

            // Transitions:
            if (init_submits)
            {
                init_submits = false;
                res = vkEndCommandBuffer(frame.init_commandbuffer);
                assert(res == VK_SUCCESS);
                queues[static_cast<uint32_t>(QueueType::Graphics)].submit_cmds.push_back(frame.init_commandbuffer);
            }

            // Sync up with copyallocator before first submit
            uint64_t copySync = copy_allocator.Flush();
            if (copySync > 0)
            {
                for (uint32_t i = 0; i < static_cast<uint32_t>(QueueType::_Count); ++i)
                {
                    CommandQueue& queue = queues[i];
                    queue.submit_waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
                    queue.submit_waitSemaphores.push_back(copy_allocator.semaphore);
                    queue.submit_waitValues.push_back(copySync);
                }
                copySync = 0;
            }

            const uint32_t cmd_last = cmd_count;
            cmd_count = 0;

            for (uint32_t cmd_index = 0; cmd_index < cmd_last; ++cmd_index)
            {
                CommandList_Vulkan& commandlist = *commandlists[cmd_index].get();
                res = vkEndCommandBuffer(commandlist.GetCommandBuffer());
                assert(res == VK_SUCCESS);

                CommandQueue& queue = queues[static_cast<uint32_t>(commandlist.queue)];
                queue.submit_cmds.push_back(commandlist.GetCommandBuffer());

                for (auto& swapchain : commandlist.prev_swapchains)
                {
                    auto swapchain_internal = ToInternal(&swapchain);
                    queue.submit_swapchains.push_back(swapchain_internal->swapchain);
                    queue.submit_swapChainImageIndices.push_back(swapchain_internal->image_index);
                    queue.submit_waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    queue.submit_waitSemaphores.push_back(swapchain_internal->semaphore_aquire);
                    queue.submit_waitValues.push_back(0);   // Not a timeline semaphore
                    queue.submit_signalSemaphores.push_back(swapchain_internal->semaphore_release);
                    queue.submit_signalValues.push_back(0); // Not a timeline semaphore
                }
            }

            for (uint32_t i = 0; i < static_cast<uint32_t>(QueueType::_Count); ++i)
            {
                queues[i].Submit(this, frame.fence[i]);
            }
        }

        frameCount++;

        // Begin next frame:
        {
            auto& frame = GetFrameResources();

            if (frameCount >= BUFFERCOUNT) 
            {
                for (uint32_t i = 0; i < static_cast<uint32_t>(QueueType::_Count); ++i)
                {
                    VkFence& fence = frame.fence[i];
                    res =  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
                    assert(res == VK_SUCCESS);
                    res = vkResetFences(device, 1, &fence);
                    assert(res == VK_SUCCESS);
                }
            }

            allocationhandler->Update(frameCount, BUFFERCOUNT);

            // Restart transition command buffers:
            {
                res = vkResetCommandPool(device, frame.init_commandpool, 0);
                assert(res == VK_SUCCESS);

                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                res = vkBeginCommandBuffer(frame.init_commandbuffer, &beginInfo);
                assert(res == VK_SUCCESS);
            }
        }

        init_submits = false;
        init_locker.unlock();
    }

    void GraphicsDevice_Vulkan::BeginRenderPass(const SwapChain* swapchain, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        auto internal_state = ToInternal(swapchain);
        commandlist.active_renderpass = &internal_state->renderpass;
        commandlist.prev_swapchains.push_back(*swapchain);

        VkResult res = vkAcquireNextImageKHR(
            device, 
            internal_state->swapchain,
            UINT64_MAX, 
            internal_state->semaphore_aquire,
            VK_NULL_HANDLE, 
            &internal_state->image_index);
        assert(res == VK_SUCCESS);

        VkClearValue clearColor = {
            swapchain->desc.clearColor[0],
            swapchain->desc.clearColor[1],
            swapchain->desc.clearColor[2],
            swapchain->desc.clearColor[3],
        };

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ToInternal(&internal_state->renderpass)->renderpass;
        renderPassInfo.framebuffer = internal_state->framebuffers[internal_state->image_index];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = internal_state->extent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandlist.GetCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void GraphicsDevice_Vulkan::BeginRenderPass(const RenderPass* renderpass, CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        commandlist.active_renderpass = renderpass;

        auto internal_state = ToInternal(renderpass);
        vkCmdBeginRenderPass(commandlist.GetCommandBuffer(), &internal_state->begin_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    void GraphicsDevice_Vulkan::EndRenderPass(CommandList cmd)
    {
        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        assert(commandlist.active_renderpass != nullptr);

        vkCmdEndRenderPass(commandlist.GetCommandBuffer());
        commandlist.active_renderpass = nullptr;
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
            vkCmdWriteTimestamp(commandlist.GetCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, internal_state->pool, index);
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

    void GraphicsDevice_Vulkan::SetName(GPUResource* resource, const char* name)
    {
        if (!debugUtils)
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

        VkResult res = vkSetDebugUtilsObjectNameEXT(device, &info);
        assert(res == VK_SUCCESS);
    }

    void GraphicsDevice_Vulkan::BeginEvent(const char* name, CommandList cmd)
    {
        if (!debugUtils)
            return;

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        const uint64_t hash = hash::StringHash(name);

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
        if (!debugUtils)
            return;

        CommandList_Vulkan& commandlist = GetCommandList(cmd);
        vkCmdEndDebugUtilsLabelEXT(commandlist.GetCommandBuffer());
    }
}
