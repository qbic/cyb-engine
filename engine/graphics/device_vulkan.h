#pragma once
#include <array>
#include <deque>
#include <mutex>
#include "core/spinlock.h"
#include "graphics/device.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "volk.h"
#include "vk_mem_alloc.h"

namespace cyb::rhi
{
    struct Queue_Vulkan
    {
		VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        uint64_t lastSubmittedID = 0;
        VkSemaphore trackingSemaphore = VK_NULL_HANDLE;

        std::vector<VkSemaphoreSubmitInfo> submit_signalSemaphoreInfos;
        std::vector<VkSemaphoreSubmitInfo> submit_waitSemaphoreInfos;
        std::vector<VkCommandBufferSubmitInfo> submit_cmds;

        void AddWaitSemaphore(VkSemaphore semaphore, uint64_t value);
        void AddSignalSemaphore(VkSemaphore semaphore, uint64_t value);
        uint64_t Submit(VkFence fence);

		uint64_t UpdateLastFinishedID();
		uint64_t GetLastFinishedID() const { return m_lastFinishedID; }

        bool PollCommandList(uint64_t commandListID);
        bool WaitCommandList(uint64_t commandListID, uint64_t timeout);

    private:
        std::recursive_mutex m_mutex;
		uint64_t m_lastFinishedID = 0;
    };

    class GraphicsDevice_Vulkan final : public GraphicsDevice
    {
    private:
        struct
        {
            bool EXT_debug_utils = false;
            bool EXT_debug_report = false;
            bool EXT_depth_clip_enable = false;
            bool EXT_conservative_rasterization = false;
            bool KHR_fragment_shading_rate = false;
            bool EXT_conditional_rendering = false;
            bool KHR_video_queue = false;
            bool KHR_video_decode_queue = false;
            bool KHR_video_decode_h264 = false;
        } extensions;

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;

        uint32_t m_graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t m_computeQueueFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t m_transferQueueFamily = VK_QUEUE_FAMILY_IGNORED;
		uint32_t m_presentFamily = VK_QUEUE_FAMILY_IGNORED;

        VkPhysicalDeviceProperties2 properties2{};
        VkPhysicalDeviceVulkan11Properties properties_1_1{};
        VkPhysicalDeviceVulkan12Properties properties_1_2{};
        VkPhysicalDeviceVulkan13Properties properties_1_3{};
        VkPhysicalDeviceMemoryProperties2 memory_properties_2{};

        VkPhysicalDeviceFeatures2 features2{};
        VkPhysicalDeviceVulkan11Features features_1_1{};
        VkPhysicalDeviceVulkan12Features features_1_2{};
        VkPhysicalDeviceVulkan13Features features_1_3{};

        std::vector<VkDynamicState> pso_dynamic_states;
        VkPipelineDynamicStateCreateInfo dynamic_state_info{};


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
                BufferHandle uploadBuffer;
                inline bool IsValid() const { return transferCommandBuffer != VK_NULL_HANDLE; }
            };

            std::vector<CopyCMD> freelist;

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            CopyCMD Allocate(uint64_t staging_size);
            void Submit(CopyCMD cmd);
        };
        mutable CopyAllocator m_copyAllocator;

        struct DescriptorBinder
        {
            DescriptorBindingTable table;
            GraphicsDevice_Vulkan* device = nullptr;

            std::vector<VkWriteDescriptorSet> descriptorWrites;
            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<VkDescriptorImageInfo> imageInfos;

            std::array<uint32_t, DESCRIPTORBINDER_CBV_COUNT> uniformBufferDynamicOffsets = {};

            VkDescriptorSet descriptorsetGraphics = VK_NULL_HANDLE;
            VkDescriptorSet descriptorsetCompute = VK_NULL_HANDLE;

            enum DIRTY_FLAGS
            {
                DIRTY_NONE       = 0,
                DIRTY_DESCRIPTOR = BIT(1),
                DIRTY_OFFSET     = BIT(2),
                DIRTY_ALL        = ~0,
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
            uint32_t poolSize = 256;

            void Init(GraphicsDevice_Vulkan* device);
            void Destroy();
            void Reset();
        };

        struct CommandList_Vulkan
        {
            std::array<std::array<VkCommandPool, uint32_t(CommandQueue::Count)>, BUFFERCOUNT> commandpools{};
            std::array<std::array<VkCommandBuffer, uint32_t(CommandQueue::Count)>, BUFFERCOUNT> commandbuffers{};
            uint32_t buffer_index = 0;

            CommandQueue queue = CommandQueue::Count;

            DescriptorBinder binder;
            std::array<DescriptorBinderPool, BUFFERCOUNT> binder_pools;
            std::array<GPULinearAllocator, BUFFERCOUNT> frame_allocators;

            std::vector<std::pair<size_t, VkPipeline>> pipelinesWorker;
            size_t prevPipelineHash = 0;
            std::vector<ISwapchain*> prevSwapchains;

            const IPipelineState* active_pso = nullptr;
            std::array<uint32_t, 8> vertexbuffer_strides{};
            size_t vertexbuffer_hash = 0;
            bool dirty_pso = false;
            RenderPassInfo renderpassInfo{};
            std::vector<VkImageMemoryBarrier2> renderpassBarriersBegin;
            std::vector<VkImageMemoryBarrier2> renderpassBarriersEnd;

            inline VkCommandPool GetCommandPool() const {
                return commandpools[buffer_index][uint32_t(queue)];
            }
            inline VkCommandBuffer GetCommandBuffer() const {
                return commandbuffers[buffer_index][uint32_t(queue)];
            }

            void Reset(uint32_t newBufferIndex) {
                buffer_index = newBufferIndex;
                binder.Reset();
                binder_pools[buffer_index].Reset();
                frame_allocators[buffer_index].Reset();
                prevPipelineHash = 0;
                active_pso = nullptr;
                vertexbuffer_hash = 0;
                vertexbuffer_strides.fill(0);
                dirty_pso = false;
                prevSwapchains.clear();
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

        void SetFenceName(VkFence fence, const char* name);
        void SetSemaphoreName(VkSemaphore semaphore, const char* name);

    public:
        GraphicsDevice_Vulkan();
        virtual ~GraphicsDevice_Vulkan();

        [[nodiscard]] Queue_Vulkan& GetQueue(CommandQueue queueIndex);

        SwapchainHandle CreateSwapchain(const SwapchainDesc* desc, NativeWindowHandle window) const override;
        BufferHandle CreateBuffer(const BufferDesc* desc, const void* initData) const override;
        QueryHandle CreateQuery(const QueryDesc* desc) const override;
        EventQueryHandle CreateEventQuery() const override;
        TextureHandle CreateTexture(const TextureDesc* desc, const SubresourceData* init_data) const override;
        ShaderHandle CreateShader(const ShaderDesc* desc) const override;
        SamplerHandle CreateSampler(const SamplerDesc* desc) const override;
        PipelineStateHandle CreatePipelineState(const PipelineStateDesc* desc) const override;
        void CreateSubresource(ITexture* texture, SubresourceType type, uint32_t firstSlice, uint32_t sliceCount, uint32_t firstMip, uint32_t mipCount) const;

        CommandList BeginCommandList(CommandQueue queue) override;
        void ExecuteCommandLists() override;
        void WaitForGPU() const override;

        void ClearPipelineStateCache() override;
        MemoryUsage GetMemoryUsage() const override;

        /////////////// Thread-sensitive ////////////////////////

        void BeginRenderPass(ISwapchain* swapchain, CommandList cmd) override;
        void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd) override;
        void EndRenderPass(CommandList cmd) override;

        void BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd) override;
        void BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd) override;
        void BindPipelineState(const IPipelineState* pso, CommandList cmd) override;
        void BindVertexBuffers(const IBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd) override;
        void BindIndexBuffer(const IBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd) override;
        void BindStencilRef(uint32_t value, CommandList cmd) override;
        void BindResource(const IResource* resource, int slot, CommandList cmd) override;
        void BindSampler(const ISampler* sampler, uint32_t slot, CommandList cmd) override;
        void BindConstantBuffer(const IBuffer* buffer, uint32_t slot, CommandList cmd, uint64_t offset) override;

        void CopyBuffer(const IBuffer* dst, uint64_t dst_offset, const IBuffer* src, uint64_t src_offset, uint64_t size, CommandList cmd) override;

        void Draw(uint32_t vertexCount, uint32_t startVertexLocation, CommandList cmd) override;
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation, CommandList cmd) override;

        void BeginQuery(IQuery* query, uint32_t index, CommandList cmd) override;
        void EndQuery(IQuery* query, uint32_t index, CommandList cmd) override;
        void ResolveQuery(const IQuery* query, uint32_t index, uint32_t count, IBuffer* dest, uint64_t destOffset, CommandList cmd) override;
        void ResetQuery(IQuery* query, uint32_t index, uint32_t count, CommandList cmd) override;
        
        void SetEventQuery(IEventQuery* query, CommandQueue queue) override;
        bool PollEventQuery(IEventQuery* query) override;
        void WaitEventQuery(IEventQuery* query) override;
        void ResetEventQuery(IEventQuery* query) override;

        void PushConstants(const void* data, uint32_t size, CommandList cmd, uint32_t offset) override;

        void BeginEvent(const char* name, CommandList cmd) override;
        void EndEvent(CommandList cmd) override;

        uint64_t GetMinOffsetAlignment(const BufferDesc* desc) const override;

        GPULinearAllocator& GetFrameAllocator(CommandList cmd) override
        {
            return GetCommandList(cmd).frame_allocators[GetBufferIndex()];
        }

        void AddWaitSemaphore(CommandQueue queueIndex, VkSemaphore semaphore, uint64_t value)
        {
            auto& queue = queues[uint32_t(queueIndex)];
            queue.AddWaitSemaphore(semaphore, value);
        }
        void AddSignalSemaphore(CommandQueue queueIndex, VkSemaphore semaphore, uint64_t value)
        {
            auto& queue = queues[uint32_t(queueIndex)];
            queue.AddSignalSemaphore(semaphore, value);
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

            // deferred destroy of resources that the GPU is already finished with
            void Update(uint64_t frameCount, uint32_t BUFFERCOUNT)
            {
                std::scoped_lock lock{ destroylocker };

                const auto destroy = [&](auto&& queue, auto&& handler) {
                    while (!queue.empty()) {
                        if (queue.front().second + BUFFERCOUNT >= frameCount)
                            break;

                        auto item = queue.front();
                        queue.pop_front();
                        handler(item.first);
                    }
                };

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
            }
        };

        std::array<Queue_Vulkan, uint32_t(CommandQueue::Count)> queues;
        std::shared_ptr<AllocationHandler> m_allocationHandler;
    };
}