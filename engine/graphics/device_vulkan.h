#pragma once
#include <array>
#include <deque>
#include <mutex>
#include <unordered_map>
#include "core/spinlock.h"
#include "graphics/device.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "volk.h"
#include "vk_mem_alloc.h"

namespace cyb::rhi::vulkan
{
    struct GraphicsDevice_Vulkan;

    struct DescriptorBinder
    {
        GraphicsDevice_Vulkan* device = nullptr;
        DescriptorBindingTable table{};

        std::vector<VkWriteDescriptorSet> descriptorWrites;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;

        std::array<uint32_t, DESCRIPTORBINDER_CBV_COUNT> uniformBufferDynamicOffsets{};

        VkDescriptorSet descriptorsetGraphics = VK_NULL_HANDLE;
        VkDescriptorSet descriptorsetCompute = VK_NULL_HANDLE;

        enum DIRTY_FLAGS
        {
            DIRTY_NONE = 0,
            DIRTY_DESCRIPTOR = BIT(1),
            DIRTY_OFFSET = BIT(2),
            DIRTY_ALL = ~0,
        };
        uint32_t dirtyFlags = DIRTY_NONE;

        void Init(GraphicsDevice_Vulkan* device);
        void Reset();
        void Flush(ICommandList* cmd);
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

    struct CommandList : public RefCounter<ICommandList>
    {
        std::vector<std::array<VkCommandPool, uint32_t(CommandQueue::Count)>> commandpools{};
        std::vector<std::array<VkCommandBuffer, uint32_t(CommandQueue::Count)>> commandbuffers{};
        uint32_t buffer_index = 0;

        CommandQueue queue = CommandQueue::Count;

        DescriptorBinder binder{};
        std::vector<DescriptorBinderPool> binder_pools;

        std::vector<std::pair<size_t, VkPipeline>> pipelinesWorker;
        size_t prevPipelineHash = 0;
        std::vector<ISwapchain*> prevSwapchains;

        const IPipelineState* activePso = nullptr;
        std::array<uint32_t, 8> vertexbuffer_strides{};
        size_t vertexbuffer_hash = 0;
        bool dirtyPso = false;
        RenderPassInfo renderpassInfo{};
        std::vector<VkImageMemoryBarrier2> renderpassBarriersBegin;
        std::vector<VkImageMemoryBarrier2> renderpassBarriersEnd;

        explicit CommandList(const GraphicsDevice_Vulkan* device, uint32_t bufferCount)
            : m_device(device)
            , commandpools(bufferCount)
            , commandbuffers(bufferCount)
            , binder_pools(bufferCount)
        {
            m_frameAllocators.reserve(bufferCount);
            for (uint32_t i = 0; i < bufferCount; i++)
                m_frameAllocators.emplace_back(std::make_unique<GPULinearAllocator>(device));
        }

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
            m_frameAllocators[buffer_index]->Reset();
            prevPipelineHash = 0;
            activePso = nullptr;
            vertexbuffer_hash = 0;
            vertexbuffer_strides.fill(0);
            dirtyPso = false;
            prevSwapchains.clear();
        }

        GPULinearAllocator* GetFrameAllocator() override;

        void BeginRenderPass(ISwapchain* swapchain) override;
        void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount) override;
        void EndRenderPass() override;

        void BindScissorRects(const Rect* rects, uint32_t rectCount) override;
        void BindViewports(const Viewport* viewports, uint32_t viewportCount) override;
        void BindPipelineState(const IPipelineState* pso) override;
        void BindVertexBuffers(const IBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets) override;
        void BindIndexBuffer(const IBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset) override;
        void BindStencilRef(uint32_t value) override;
        void BindResource(const IResource* resource, int slot) override;
        void BindSampler(const ISampler* sampler, uint32_t slot) override;
        void BindConstantBuffer(const IBuffer* buffer, uint32_t slot, uint64_t offset) override;

        void CopyBuffer(const IBuffer* dst, uint64_t dstOffset, const IBuffer* src, uint64_t srcOffset, uint64_t size) override;
        void UpdateBuffer(IBuffer* buffer, const void* data, uint64_t size, uint64_t offset) override;

        void Draw(uint32_t vertexCount, uint32_t startVertexLocation) override;
        void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) override;

        void BeginMarker(std::string_view name) const override;
        void EndMarker() const override;

    private:
        void ValidatePSO();
        void PreDraw();

        const GraphicsDevice_Vulkan* m_device = nullptr;
        std::vector<std::unique_ptr<GPULinearAllocator>> m_frameAllocators;
    };

    class Queue
    {
    public:
        VkSemaphore trackingSemaphore = VK_NULL_HANDLE;
        std::vector<VkCommandBufferSubmitInfo> submit_cmds;

        Queue(VkDevice device, CommandQueue queueID, VkQueue queue, uint32_t queueFamilyIndex);
        ~Queue();

        void AddWaitSemaphore(VkSemaphore semaphore, uint64_t value);
        void AddSignalSemaphore(VkSemaphore semaphore, uint64_t value);
        uint64_t Submit();

		uint64_t UpdateLastFinishedID();
        [[nodiscard]] uint64_t GetLastSubmittedID() const { return m_lastSubmittedID; }
		[[nodiscard]] uint64_t GetLastFinishedID() const { return m_lastFinishedID; }

        bool PollCommandList(uint64_t commandListID);
        bool WaitCommandList(uint64_t commandListID, uint64_t timeout);

        VkQueue GetVkQueue() const { return m_queue; }

    private:
        VkDevice m_device;
        VkQueue m_queue;
        CommandQueue m_queueID;
        uint32_t m_queueFamilyIndex;

        std::recursive_mutex m_mutex;
        std::vector<VkSemaphoreSubmitInfo> m_waitSemaphoreInfos;
        std::vector<VkSemaphoreSubmitInfo> m_signalSemaphoreInfos;
        
        uint64_t m_lastSubmittedID = 0;
        uint64_t m_lastFinishedID = 0;
    };

    class GraphicsDevice_Vulkan final : public GraphicsDevice
    {
    public:
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

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceMemoryProperties memoryProperties;
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR fragmentShadingRateProperties;
        VkPhysicalDeviceFeatures features;
        
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
                uint64_t submissionID = 0;
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

        std::vector<CommandListHandle> m_commandlists;
        uint32_t m_cmdCount = 0;
        SpinLock m_cmdLocker;

        struct PSOLayout
        {
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            VkDescriptorSetLayout descriptorset_layout = VK_NULL_HANDLE;
        };
        mutable std::unordered_map<size_t, PSOLayout> m_psoLayoutCache;
        mutable std::mutex m_psoLayoutCacheMutex;

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        std::unordered_map<size_t, VkPipeline> pipelinesGlobal;

        void SetFenceName(VkFence fence, const char* name);
        void SetSemaphoreName(VkSemaphore semaphore, const char* name);

    public:
        GraphicsDevice_Vulkan();
        virtual ~GraphicsDevice_Vulkan();

        [[nodiscard]] Queue* GetQueue(CommandQueue queue) const { return queues[uint32_t(queue)].get(); }

        SwapchainHandle CreateSwapchain(const SwapchainDesc* desc, NativeWindowHandle window) const override;
        BufferHandle CreateBuffer(const BufferDesc* desc, const void* initData) const override;
        QueryHandle CreateQuery(const QueryDesc* desc) const override;
        EventQueryHandle CreateEventQuery() const override;
        TextureHandle CreateTexture(const TextureDesc* desc, const SubresourceData* init_data) const override;
        ShaderHandle CreateShader(const ShaderDesc* desc) const override;
        SamplerHandle CreateSampler(const SamplerDesc* desc) const override;
        PipelineStateHandle CreatePipelineState(const PipelineStateDesc* desc) const override;
        void CreateSubresource(ITexture* texture, SubresourceType type, uint32_t firstSlice, uint32_t sliceCount, uint32_t firstMip, uint32_t mipCount) const;

        ICommandList* BeginCommandList(CommandQueue queue) override;
        void ExecuteCommandLists() override;
        void Present(ISwapchain* swapchain) override;
        void WaitForGPU() const override;

        void ClearPipelineStateCache() override;
        MemoryUsage GetMemoryUsage() const override;

        /////////////// Thread-sensitive ////////////////////////

        void BeginQuery(IQuery* query, uint32_t index, ICommandList* cmd) override;
        void EndQuery(IQuery* query, uint32_t index, ICommandList* cmd) override;
        void ResolveQuery(const IQuery* query, uint32_t index, uint32_t count, IBuffer* dest, uint64_t destOffset, ICommandList* cmd) override;
        void ResetQuery(IQuery* query, uint32_t index, uint32_t count, ICommandList* cmd) override;
        
        void SetEventQuery(IEventQuery* query, CommandQueue queue) override;
        bool PollEventQuery(IEventQuery* query) override;
        void WaitEventQuery(IEventQuery* query) override;
        void ResetEventQuery(IEventQuery* query) override;

        void PushConstants(const void* data, uint32_t size, ICommandList* cmd, uint32_t offset) override;

        uint64_t GetMinOffsetAlignment(const BufferDesc* desc) const override;

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

        std::array<std::unique_ptr<Queue>, uint32_t(CommandQueue::Count)> queues;
        std::shared_ptr<AllocationHandler> m_allocationHandler;
    };
}