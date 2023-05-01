#pragma once
#include "core/spinlock.h"
#include "graphics/graphics-device.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "volk.h"
#include "vk_mem_alloc.h"
#include <deque>

namespace cyb::graphics
{
    class GraphicsDevice_Vulkan final : public GraphicsDevice
    {
    private:
        bool debugUtils = false;
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        std::vector<VkQueueFamilyProperties> queueFamilies;
        uint32_t graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t computeFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t copyFamily = VK_QUEUE_FAMILY_IGNORED;
        std::vector<uint32_t> families;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue computeQueue = VK_NULL_HANDLE;
        VkQueue copyQueue = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties2 properties2 = {};
        VkPhysicalDeviceVulkan11Properties properties_1_1 = {};
        VkPhysicalDeviceVulkan12Properties properties_1_2 = {};
        VkPhysicalDeviceVulkan13Properties properties_1_3 = {};
        VkPhysicalDeviceMemoryProperties2 memory_properties_2 = {};

        VkPhysicalDeviceFeatures2 features2 = {};
        VkPhysicalDeviceVulkan11Features features_1_1 = {};
        VkPhysicalDeviceVulkan12Features features_1_2 = {};
        VkPhysicalDeviceVulkan13Features features_1_3 = {};

        std::vector<VkDynamicState> pso_dynamic_states;
        VkPipelineDynamicStateCreateInfo dynamic_state_info = {};

        struct CommandQueue
        {
            VkQueue queue = VK_NULL_HANDLE;
            std::vector<VkSwapchainKHR> submit_swapchains;
            std::vector<uint32_t> submit_swapChainImageIndices;
            std::vector<VkPipelineStageFlags> submit_waitStages;
            std::vector<VkSemaphore> submit_waitSemaphores;
            std::vector<uint64_t> submit_waitValues;
            std::vector<VkSemaphore> submit_signalSemaphores;
            std::vector<uint64_t> submit_signalValues;
            std::vector<VkCommandBuffer> submit_cmds;

            void Submit(GraphicsDevice_Vulkan* device, VkFence fence);
        };
        CommandQueue queues[static_cast<uint32_t>(QueueType::_Count)];

        struct CopyAllocator
        {
            GraphicsDevice_Vulkan* device = nullptr;
            VkSemaphore semaphore = VK_NULL_HANDLE;
            uint64_t fence_value = 0;
            std::mutex locker;

            struct CopyCMD
            {
                VkCommandPool commandpool = VK_NULL_HANDLE;
                VkCommandBuffer commandbuffer = VK_NULL_HANDLE;
                uint64_t target = 0;
                GPUBuffer uploadBuffer;
            };

            std::vector<CopyCMD> freelist;              // Available
            std::vector<CopyCMD> worklist;              // In progress
            std::vector<VkCommandBuffer> submit_cmds;   // For next submit
            uint64_t submit_wait = 0;                   // Last submit wait value

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            CopyCMD Allocate(uint64_t staging_size);
            void Submit(CopyCMD cmd);
            uint64_t Flush();
        };
        mutable CopyAllocator m_copyAllocator;

        struct FrameResources
        {
            VkFence fence[static_cast<uint32_t>(QueueType::_Count)];
            VkCommandPool init_commandpool;
            VkCommandBuffer init_commandbuffer;
        };
        mutable std::mutex m_initLocker;
        mutable bool m_initSubmits = false;
        struct FrameResources m_frameResources[BUFFERCOUNT];
        const FrameResources& GetFrameResources() const { return m_frameResources[GetBufferIndex()]; }
        FrameResources& GetFrameResources() { return m_frameResources[GetBufferIndex()]; }

        struct DescriptorBinder
        {
            DescriptorBindingTable table;
            GraphicsDevice_Vulkan* device = nullptr;

            std::vector<VkWriteDescriptorSet> descriptor_writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            std::vector<VkDescriptorImageInfo> image_infos;

            uint32_t uniform_buffer_dynamic_offsets[DESCRIPTORBINDER_CBV_COUNT] = {};

            VkDescriptorSet descriptorset_graphics = VK_NULL_HANDLE;
            VkDescriptorSet descriptorset_compute = VK_NULL_HANDLE;

            enum DIRTY_FLAGS
            {
                DIRTY_NONE = 0,
                DIRTY_DESCRIPTOR = 1 << 1,
                DIRTY_OFFSET = 1 << 2,

                DIRTY_ALL = ~0,
            };
            uint32_t dirty = DIRTY_NONE;

            void Init(GraphicsDevice_Vulkan* device);
            void Reset();
            void Flush(CommandList cmd);
        };

        struct DescriptorBinderPool
        {
            GraphicsDevice_Vulkan* device = nullptr;
            VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
            uint32_t pool_max_size = 256;

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            void Reset();
        };

        struct CommandList_Vulkan
        {
            VkCommandPool commandpools[BUFFERCOUNT][static_cast<uint32_t>(QueueType::_Count)] = {};
            VkCommandBuffer commandbuffers[BUFFERCOUNT][static_cast<uint32_t>(QueueType::_Count)] = {};
            uint32_t buffer_index = 0;

            QueueType queue = QueueType::_Count;
            uint32_t id = 0;

            DescriptorBinder binder;
            DescriptorBinderPool binder_pools[BUFFERCOUNT];
            GPULinearAllocator frame_allocators[BUFFERCOUNT];

            std::vector<std::pair<size_t, VkPipeline>> pipelinesWorker;
            size_t prevPipelineHash = 0;
            std::vector<SwapChain> prev_swapchains;

            const PipelineState* active_pso = nullptr;
            uint32_t vertexbuffer_strides[8] = {};
            size_t vertexbuffer_hash = 0;
            bool dirty_pso = false;
            RenderPassInfo renderpassInfo = {};
            std::vector<VkImageMemoryBarrier> renderpassBarriersBegin;
            std::vector<VkImageMemoryBarrier> renderpassBarriersEnd;

            inline VkCommandPool GetCommandPool() const
            {
                return commandpools[buffer_index][static_cast<uint32_t>(queue)];
            }
            inline VkCommandBuffer GetCommandBuffer() const
            {
                return commandbuffers[buffer_index][static_cast<uint32_t>(queue)];
            }

            void Reset(uint32_t newBufferIndex)
            {
                buffer_index = newBufferIndex;
                binder.Reset();
                binder_pools[buffer_index].Reset();
                frame_allocators[buffer_index].Reset();
                prevPipelineHash = 0;
                active_pso = nullptr;
                vertexbuffer_hash = 0;
                for (int i = 0; i < _countof(vertexbuffer_strides); ++i)
                {
                    vertexbuffer_strides[i] = 0;
                }
                dirty_pso = false;
                prev_swapchains.clear();
            }
        };

        std::vector<std::unique_ptr<CommandList_Vulkan>> m_commandlists;
        uint32_t m_cmdCount = 0;
        SpinLock m_cmdLocker;

        constexpr CommandList_Vulkan& GetCommandList(CommandList cmd) const
        {
            assert(cmd.IsValid());
            return *(CommandList_Vulkan*)cmd.internal_state;
        }

        struct PSOLayout
        {
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            VkDescriptorSetLayout descriptorset_layout = VK_NULL_HANDLE;
        };
        mutable std::unordered_map<size_t, PSOLayout> m_psoLayoutCache;
        mutable std::mutex m_psoLayoutCacheMutex;

        VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
        std::unordered_map<size_t, VkPipeline> m_pipelinesGlobal;

        void ValidatePSO(CommandList cmds);
        void PreDraw(CommandList cmds);

    public:
        GraphicsDevice_Vulkan();
        virtual ~GraphicsDevice_Vulkan();

        bool CreateSwapChain(const SwapChainDesc* desc, platform::Window* window, SwapChain* swapchain) const override;
        bool CreateBuffer(const GPUBufferDesc* desc, const void* init_data, GPUBuffer* buffer) const override;
        bool CreateQuery(const GPUQueryDesc* desc, GPUQuery* query) const override;
        bool CreateTexture(const TextureDesc* desc, const SubresourceData* init_data, Texture* texture) const override;
        bool CreateShader(ShaderStage stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const override;
        bool CreateSampler(const SamplerDesc* desc, Sampler* sampler) const override;
        bool CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const override;
        void CreateSubresource(Texture* texture, SubresourceType type, uint32_t firstSlice, uint32_t sliceCount, uint32_t firstMip, uint32_t mipCount) const;

        CommandList BeginCommandList(QueueType queue) override;
        void SubmitCommandList() override;
        void SetName(GPUResource* pResource, const char* name) override;
        void SetName(Shader* shader, const char* name) override;

        void ClearPipelineStateCache() override;
        MemoryUsage GetMemoryUsage() const override;

        /////////////// Thread-sensitive ////////////////////////

        void BeginRenderPass(const SwapChain* swapchain, CommandList cmd) override;
        void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd) override;
        void EndRenderPass(CommandList cmd) override;

        void BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd) override;
        void BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd) override;
        void BindPipelineState(const PipelineState* pso, CommandList cmd) override;
        void BindVertexBuffers(const GPUBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd) override;
        void BindIndexBuffer(const GPUBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd) override;
        void BindStencilRef(uint32_t value, CommandList cmd) override;
        void BindResource(const GPUResource* resource, int slot, CommandList cmd) override;
        void BindSampler(const Sampler* sampler, uint32_t slot, CommandList cmd) override;
        void BindConstantBuffer(const GPUBuffer* buffer, uint32_t slot, CommandList cmd, uint64_t offset) override;

        void CopyBuffer(const GPUBuffer* dst, uint64_t dst_offset, const GPUBuffer* src, uint64_t src_offset, uint64_t size, CommandList cmd) override;

        void Draw(uint32_t vertexCount, uint32_t startVertexLocation, CommandList cmd) override;
        void DrawIndexed(uint32_t index_count, uint32_t start_index_location, int32_t base_vertex_location, CommandList cmd) override;

        void BeginQuery(const GPUQuery* query, uint32_t index, CommandList cmd) override;
        void EndQuery(const GPUQuery* query, uint32_t index, CommandList cmd) override;
        void ResolveQuery(const GPUQuery* query, uint32_t index, uint32_t count, const GPUBuffer* dest, uint64_t destOffset, CommandList cmd) override;
        void ResetQuery(const GPUQuery* query, uint32_t index, uint32_t count, CommandList cmd) override;

        void BeginEvent(const char* name, CommandList cmd) override;
        void EndEvent(CommandList cmd) override;

        uint64_t GetMinOffsetAlignment(const GPUBufferDesc* desc) const override;

        GPULinearAllocator& GetFrameAllocator(CommandList cmd) override
        {
            return GetCommandList(cmd).frame_allocators[GetBufferIndex()];
        }

        struct AllocationHandler
        {
            VmaAllocator allocator = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkInstance instance = VK_NULL_HANDLE;
            std::mutex destroylocker;
            uint64_t framecount = 0;

            std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> destroyer_images;
            std::deque<std::pair<VkImageView, uint64_t>> destroyer_imageviews;
            std::deque<std::pair<std::pair<VkBuffer, VmaAllocation>, uint64_t>> destroyer_buffers;
            std::deque<std::pair<VkBufferView, uint64_t>> destroyer_bufferviews;
            std::deque<std::pair<VkQueryPool, uint64_t>> destroyer_querypools;
            std::deque<std::pair<VkSampler, uint64_t>> destroyer_samplers;
            std::deque<std::pair<VkDescriptorPool, uint64_t>> destroyer_descriptorPools;
            std::deque<std::pair<VkDescriptorSetLayout, uint64_t>> destroyer_descriptorSetLayouts;
            std::deque<std::pair<VkShaderModule, uint64_t>> destroyer_shadermodules;
            std::deque<std::pair<VkPipelineLayout, uint64_t>> destroyer_pipelineLayouts;
            std::deque<std::pair<VkPipeline, uint64_t>> destroyer_pipelines;
            std::deque<std::pair<VkSwapchainKHR, uint64_t>> destroyer_swapchains;
            std::deque<std::pair<VkSurfaceKHR, uint64_t>> destroyer_surfaces;
            std::deque<std::pair<VkSemaphore, uint64_t>> destroyer_semaphores;

            ~AllocationHandler()
            {
                Update(~0, 0); // destroy all remaining
                vmaDestroyAllocator(allocator);
                vkDestroyDevice(device, nullptr);
                vkDestroyInstance(instance, nullptr);
            }

            // Deferred destroy of resources that the GPU is already finished with:
            void Update(uint64_t frameCount, uint32_t BUFFERCOUNT)
            {
                destroylocker.lock();
                framecount = frameCount;

                while (!destroyer_images.empty())
                {
                    if (destroyer_images.front().second + BUFFERCOUNT >= frameCount)
                        break;

                    auto& item = destroyer_images.front();
                    destroyer_images.pop_front();
                    vmaDestroyImage(allocator, item.first.first, item.first.second);
                }
                while (!destroyer_imageviews.empty())
                {
                    if (destroyer_imageviews.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_imageviews.front();
                        destroyer_imageviews.pop_front();
                        vkDestroyImageView(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_buffers.empty())
                {
                    if (destroyer_buffers.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_buffers.front();
                        destroyer_buffers.pop_front();
                        vmaDestroyBuffer(allocator, item.first.first, item.first.second);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_bufferviews.empty())
                {
                    if (destroyer_bufferviews.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_bufferviews.front();
                        destroyer_bufferviews.pop_front();
                        vkDestroyBufferView(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_querypools.empty())
                {
                    if (destroyer_querypools.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_querypools.front();
                        destroyer_querypools.pop_front();
                        vkDestroyQueryPool(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_samplers.empty())
                {
                    if (destroyer_samplers.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_samplers.front();
                        destroyer_samplers.pop_front();
                        vkDestroySampler(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_descriptorPools.empty())
                {
                    if (destroyer_descriptorPools.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_descriptorPools.front();
                        destroyer_descriptorPools.pop_front();
                        vkDestroyDescriptorPool(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_descriptorSetLayouts.empty())
                {
                    if (destroyer_descriptorSetLayouts.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_descriptorSetLayouts.front();
                        destroyer_descriptorSetLayouts.pop_front();
                        vkDestroyDescriptorSetLayout(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_shadermodules.empty())
                {
                    if (destroyer_shadermodules.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_shadermodules.front();
                        destroyer_shadermodules.pop_front();
                        vkDestroyShaderModule(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_pipelineLayouts.empty())
                {
                    if (destroyer_pipelineLayouts.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_pipelineLayouts.front();
                        destroyer_pipelineLayouts.pop_front();
                        vkDestroyPipelineLayout(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_pipelines.empty())
                {
                    if (destroyer_pipelines.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_pipelines.front();
                        destroyer_pipelines.pop_front();
                        vkDestroyPipeline(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_swapchains.empty())
                {
                    if (destroyer_swapchains.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_swapchains.front();
                        destroyer_swapchains.pop_front();
                        vkDestroySwapchainKHR(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_surfaces.empty())
                {
                    if (destroyer_surfaces.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_surfaces.front();
                        destroyer_surfaces.pop_front();
                        vkDestroySurfaceKHR(instance, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }
                while (!destroyer_semaphores.empty())
                {
                    if (destroyer_semaphores.front().second + BUFFERCOUNT < frameCount)
                    {
                        auto& item = destroyer_semaphores.front();
                        destroyer_semaphores.pop_front();
                        vkDestroySemaphore(device, item.first, nullptr);
                    }
                    else
                    {
                        break;
                    }
                }

                destroylocker.unlock();
            }
        };

        std::shared_ptr<AllocationHandler> m_allocationHandler;
    };
}