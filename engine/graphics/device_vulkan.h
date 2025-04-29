#pragma once
#include <deque>
#include <mutex>
#include "core/spinlock.h"
#include "graphics/device.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "volk.h"
#include "vk_mem_alloc.h"

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
            std::vector<uint32_t> submit_swapchainImageIndices;
            std::vector<VkSemaphore> submit_signalSemaphores;
            std::vector<VkSemaphoreSubmitInfo> submit_signalSemaphoreInfos;
            std::vector<VkSemaphoreSubmitInfo> submit_waitSemaphoreInfos;
            std::vector<VkCommandBufferSubmitInfo> submit_cmds;

            std::shared_ptr<std::mutex> locker;

            void Submit(GraphicsDevice_Vulkan* device, VkFence fence);
        };
        CommandQueue queues[static_cast<uint32_t>(QueueType::_Count)];

        struct CopyAllocator
        {
            GraphicsDevice_Vulkan* device = nullptr;
            std::mutex locker;

            struct CopyCMD
            {
                VkCommandPool transferCommandPool = VK_NULL_HANDLE;
                VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;
                VkCommandPool transitionCommandPool = VK_NULL_HANDLE;
                VkCommandBuffer transitionCommandBuffer = VK_NULL_HANDLE;
                VkFence fence = VK_NULL_HANDLE;
                VkSemaphore semaphores[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE }; // graphics, compute
                GPUBuffer uploadBuffer;
                inline bool IsValid() const { return transferCommandBuffer != VK_NULL_HANDLE; }
            };

            std::vector<CopyCMD> freelist;

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            CopyCMD Allocate(uint64_t staging_size);
            void Submit(CopyCMD cmd);
        };
        mutable CopyAllocator m_copyAllocator;
        VkFence m_frameFence[BUFFERCOUNT][NumericalValue(QueueType::_Count)] = {};

        struct DescriptorBinder
        {
            DescriptorBindingTable table;
            GraphicsDevice_Vulkan* device = nullptr;

            std::vector<VkWriteDescriptorSet> descriptorWrites;
            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<VkDescriptorImageInfo> imageInfos;

            uint32_t uniformBufferDynamicOffsets[DESCRIPTORBINDER_CBV_COUNT] = {};

            VkDescriptorSet descriptorsetGraphics = VK_NULL_HANDLE;
            VkDescriptorSet descriptorsetCompute = VK_NULL_HANDLE;

            enum DIRTY_FLAGS
            {
                DIRTY_NONE = 0,
                DIRTY_DESCRIPTOR = 1 << 1,
                DIRTY_OFFSET = 1 << 2,
                DIRTY_ALL = ~0,
            };
            uint32_t dirtyFlags = DIRTY_NONE;

            void Init(GraphicsDevice_Vulkan* device);
            void Reset();
            void Flush(CommandList cmd);
        };

        struct DescriptorBinderPool
        {
            GraphicsDevice_Vulkan* device = nullptr;
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
            uint32_t poolMaxSize = 256;

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            void Reset();
        };

        struct CommandList_Vulkan {
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
            std::vector<Swapchain> prevSwapchains;

            const PipelineState* active_pso = nullptr;
            uint32_t vertexbuffer_strides[8] = {};
            size_t vertexbuffer_hash = 0;
            bool dirty_pso = false;
            RenderPassInfo renderpassInfo = {};
            std::vector<VkImageMemoryBarrier2> renderpassBarriersBegin;
            std::vector<VkImageMemoryBarrier2> renderpassBarriersEnd;
#ifdef CYB_DEBUG_BUILD
            int32_t eventCount = 0;
#endif
            inline VkCommandPool GetCommandPool() const {
                return commandpools[buffer_index][static_cast<uint32_t>(queue)];
            }
            inline VkCommandBuffer GetCommandBuffer() const {
                return commandbuffers[buffer_index][static_cast<uint32_t>(queue)];
            }

            void Reset(uint32_t newBufferIndex) {
                buffer_index = newBufferIndex;
                binder.Reset();
                binder_pools[buffer_index].Reset();
                frame_allocators[buffer_index].Reset();
                prevPipelineHash = 0;
                active_pso = nullptr;
                vertexbuffer_hash = 0;
                for (int i = 0; i < _countof(vertexbuffer_strides); ++i) {
                    vertexbuffer_strides[i] = 0;
                }
                dirty_pso = false;
                prevSwapchains.clear();
#ifdef CYB_DEBUG_BUILD
                eventCount = 0;
#endif
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

        bool CreateSwapchain(const SwapchainDesc* desc, WindowHandle window, Swapchain* swapchain) const override;
        bool CreateBuffer(const GPUBufferDesc* desc, const void* initData, GPUBuffer* buffer) const override;
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

        void BeginRenderPass(const Swapchain* swapchain, CommandList cmd) override;
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
        
        void PushConstants(const void* data, uint32_t size, CommandList cmd, uint32_t offset) override;

        void BeginEvent(const char* name, CommandList cmd) override;
        void EndEvent(CommandList cmd) override;

        uint64_t GetMinOffsetAlignment(const GPUBufferDesc* desc) const override;

        GPULinearAllocator& GetFrameAllocator(CommandList cmd) override
        {
            return GetCommandList(cmd).frame_allocators[GetBufferIndex()];
        }

        struct AllocationHandler {
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

            ~AllocationHandler() {
                Update(~0, 0); // destroy all remaining
                vmaDestroyAllocator(allocator);
                vkDestroyDevice(device, nullptr);
                vkDestroyInstance(instance, nullptr);
            }

            // Deferred destroy of resources that the GPU is already finished with:
            void Update(uint64_t frameCount, uint32_t BUFFERCOUNT) {
                const auto destroy = [&](auto&& queue, auto&& handler) {
                    while (!queue.empty()) {
                        if (queue.front().second + BUFFERCOUNT >= frameCount)
                            break;

                        auto item = queue.front();
                        queue.pop_front();
                        handler(item.first);
                    }
                };

                destroylocker.lock();
                framecount = frameCount;

                destroy(destroyer_images, [&](auto& item) {
                    vmaDestroyImage(allocator, item.first, item.second);
                });
                destroy(destroyer_imageviews, [&](auto& item) {
                    vkDestroyImageView(device, item, nullptr);
                });
                destroy(destroyer_buffers, [&](auto& item) {
                    vmaDestroyBuffer(allocator, item.first, item.second);
                });
                destroy(destroyer_bufferviews, [&](auto& item) {
                    vkDestroyBufferView(device, item, nullptr);
                });
                destroy(destroyer_querypools, [&](auto& item) {
                    vkDestroyQueryPool(device, item, nullptr);
                });
                destroy(destroyer_samplers, [&](auto& item) {
                    vkDestroySampler(device, item, nullptr);
                });
                destroy(destroyer_descriptorPools, [&](auto& item) {
                    vkDestroyDescriptorPool(device, item, nullptr);
                });
                destroy(destroyer_descriptorSetLayouts, [&](auto& item) {
                    vkDestroyDescriptorSetLayout(device, item, nullptr);
                });
                destroy(destroyer_shadermodules, [&](auto& item) {
                    vkDestroyShaderModule(device, item, nullptr);
                });
                destroy(destroyer_pipelineLayouts, [&](auto& item) {
                    vkDestroyPipelineLayout(device, item, nullptr);
                });
                destroy(destroyer_pipelines, [&](auto& item) {
                    vkDestroyPipeline(device, item, nullptr);
                });
                destroy(destroyer_swapchains, [&](auto& item) {
                    vkDestroySwapchainKHR(device, item, nullptr);
                });
                destroy(destroyer_surfaces, [&](auto& item) {
                    vkDestroySurfaceKHR(instance, item, nullptr);
                });
                destroy(destroyer_semaphores, [&](auto& item) {
                    vkDestroySemaphore(device, item, nullptr);
                });

                destroylocker.unlock();
            }
        };

        std::shared_ptr<AllocationHandler> m_allocationHandler;
    };
}