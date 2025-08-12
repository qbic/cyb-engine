#pragma once
#include <array>
#include <vector>
#include "core/platform.h"
#include "core/mathlib.h"
#include "core/logger.h"

namespace cyb::rhi
{
    struct Texture;
    struct Shader;

    enum class BufferUsage : uint8_t
    {
        None                = 0,
        VertexBufferBit     = BIT(0),
        IndexBufferBit      = BIT(1),
        ConstantBufferBit   = BIT(2),
    };
    CYB_ENABLE_BITMASK_OPERATORS(BufferUsage);

    enum class CpuAccessMode : uint8_t
    {
        None,                           //!< CPU no access, GPU read/write
        Read,                           //!< CPU write, GPU read
        Write                           //!< CPU, GPU write
    };

    enum class SamplerAddressMode : uint8_t
    {
        Clamp,
        Wrap,
        Mirror,
        Border
    };

    enum class ComponentSwizzle : uint8_t
    {
        Zero,
        One,
        R,
        G,
        B,
        A
    };

    enum class Format : uint8_t
    {
        Unknown,

        R8_UNORM,                       //!< Single-component, 8-bit unsigned-normalized integer swizzeled to { r, r, r, 1 }
        RGBA8_UINT,                     //!< Four-component, 32-bit unsigned-integer format with 8-bit channels
        RGBA8_UNORM,                    //!< Four-component, 32-bit unsigned-normalized integer format with 8-bit channels
        BGRA8_UNORM,
        R16_FLOAT,                      //!< Single-component, 16-bit floating-point format swizzeled to { r, r, r, 1 }
        RG16_FLOAT,                     //!< Two-component, 32-bit floating-point format with 16-bit channels 
        R32_FLOAT,                      //!< Single-component, 32-bit floating-point format swizzeled to { r, r, r, 1 }
        RG32_FLOAT,                     //!< Two-component, 64-bit floating-point format with 32-bit channels
        RGB32_FLOAT,
        RGBA32_FLOAT,                   //!< Four-component, 128-bit floating-point format with 32-bit channels
        
        D24S8,                          //!< Two-component, Depth (24-bit) + stencil (8-bit)
        D32,                            //!< Single-component, 32-bit floating-point format for depth
        D32S8,                          //!< Two-component, Depth (32-bit) + stencil (8-bit) (24-bits unused)
        COUNT
    };

    struct FormatInfo
    {
        Format format;
        const char* name;
        uint8_t bytesPerBlock;
        uint8_t blockSize;
        bool hasDepth;
        bool hasStencil;
    };
    const FormatInfo& GetFormatInfo(Format format);

    enum class IndexBufferFormat : uint8_t
    {
        Uint16,
        Uint32
    };

    enum class SubresourceType : uint8_t
    {
        SRV,                            //!< Shader resource view
        RTV,                            //!< Render target view
        DSV                             //!< Depth stencil view
    };

    enum class PolygonMode : uint8_t
    {
        Fill,
        Line,
        Point
    };

    enum class CullMode : uint8_t
    {
        None,
        Front,
        Back
    };

    enum class FrontFace : uint8_t
    {
        CCW,                            // Counterclockwise
        CW                              // Clockwise
    };

    enum class PrimitiveTopology : uint8_t
    {
        TriangleList,
        TriangleStrip,
        PointList,
        LineList,
        LineStrip
    };

    enum class ComparisonFunc : uint8_t
    {
        Never,
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Allways
    };

    enum class DepthWriteMask : uint8_t
    {
        Zero,                           // Disables depth write
        All,                            // Enables depth write
    };

    enum class StencilOp : uint8_t
    {
        Keep,
        Zero,
        Replace,
        IncrementClamp,
        DecrementClamp,
        Invert,
        Increment,
        Decrement,
    };

    enum class ShaderType : uint8_t
    {
        Vertex,
        Pixel,
        Geometry,
        Count
    };

    enum class ShaderFormat : uint8_t
    {
        None,
        SpirV,
        GLSL
    };

    enum class QueueType : uint8_t
    {
        Graphics,
        Compute,
        Copy,
        Count
    };

    enum class ResourceStates : uint32_t
    {
        // Common resource states:
        Unknown             = 0,        //!< Dont preserve contents
        ShaderResourceBit   = BIT(0),   //!< Shader resource, read only
        UnorderedAccessBit  = BIT(1),   //!< Shader resource, write enabled
        CopySourceBit       = BIT(2),   //!< Copy from
        CopyDestBit         = BIT(3),   //!< Copy to

        // Texture specific resource states:
        RenderTargetBit     = BIT(10),  //!< Render target, write enabled
        DepthWriteBit       = BIT(11),  //!< Depth stencil, write enabled
        DepthReadBit        = BIT(12),  //!< Depth stencil, read only

        // GPUBuffer specific resource states:
        VertexBufferBit     = BIT(20),  //!< Vertex buffer, read only
        IndexBufferBit      = BIT(21),  //!< Index buffer, read only
        ConstantBufferBit   = BIT(22),  //!< Constant buffer, read only
        IndirectArgumentBit = BIT(23),  //!< Argument buffer to DrawIndirect() or DispatchIndirect()
        AccelStructBit      = BIT(24),  //!< Acceleration structure storage or scratch
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceStates);

    enum class GPUQueryType : uint8_t
    {
        Timestamp,                      //!< Retrieve time point of gpu execution
        Occlusion,                      //!< How many samples passed depth test?
        OcclusionBinary                 //!< Depth test passed or not?
    };

    struct GPUBufferDesc
    {
        uint64_t size = 0;
        CpuAccessMode cpuAccess = CpuAccessMode::None;
        BufferUsage usage = BufferUsage::None;
        uint32_t stride = 0;            // Needed for struct buffer types
    };

    struct GPUQueryDesc
    {
        GPUQueryType type = GPUQueryType::Timestamp;
        uint32_t queryCount = 0;
    };

    struct Viewport
    {
        float x = 0;                    // Top-Left
        float y = 0;                    // Top-Left
        float width = 0;
        float height = 0;
        float minDepth = 0;
        float maxDepth = 1;
    };

    struct VertexInputLayout
    {
        static constexpr uint32_t APPEND_ALIGNMENT_ELEMENT = ~0u;

        struct Element
        {
            std::string inputName;
            uint32_t inputSlot = 0;
            Format format = Format::Unknown;

            // setting alignedByteOffset to APPEND_ALIGNMENT_ELEMENT calculates offset using format
            uint32_t alignedByteOffset = APPEND_ALIGNMENT_ELEMENT;
        };

        std::vector<Element> elements;

        VertexInputLayout() = default;
        VertexInputLayout(std::initializer_list<Element> init) :
            elements(init)
        {
        }
    };

    struct SamplerDesc
    {
        bool minFilter = true;
        bool magFilter = true;
        bool mipFilter = true;
        SamplerAddressMode addressU = SamplerAddressMode::Wrap;
        SamplerAddressMode addressV = SamplerAddressMode::Wrap;
        SamplerAddressMode addressW = SamplerAddressMode::Wrap;
        float lodBias = 0.0f;
        float maxAnisotropy = 1.0f;
        XMFLOAT4 borderColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float minLOD = 0.0f;
        float maxLOD = FLT_MAX;
    };

    struct Swizzle
    {
        ComponentSwizzle r = ComponentSwizzle::R;
        ComponentSwizzle g = ComponentSwizzle::G;
        ComponentSwizzle b = ComponentSwizzle::B;
        ComponentSwizzle a = ComponentSwizzle::A;
    };

    union ClearValue
    {
        struct ClearDepthStencil
        {
            float depth;
            uint32_t stencil;
        };

        float color[4];
        ClearDepthStencil depthStencil;
    };

    enum class TextureType : uint8_t
    {
        Unknown,
        Texture1D,          // UNTESTED
        Texture2D,
        Texture3D           // NOT IMPLEMENTED
    };

    struct TextureDesc
    {
        TextureType type = TextureType::Texture2D;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t arraySize = 1;
        Format format = Format::Unknown;
        Swizzle swizzle;
        uint32_t mipLevels = 1;
        ClearValue clear = {};
        ResourceStates initialState = ResourceStates::ShaderResourceBit;
    };

    struct RasterizerState
    {
        PolygonMode polygonMode = PolygonMode::Fill;
        CullMode cullMode = CullMode::None;
        FrontFace frontFace = FrontFace::CCW;
        float lineWidth = 1.0f;
    };

    struct DepthStencilState
    {
        struct DepthStencilOp
        {
            StencilOp stencilFailOp = StencilOp::Keep;
            StencilOp stencilDepthFailOp = StencilOp::Keep;
            StencilOp stencilPassOp = StencilOp::Keep;
            ComparisonFunc stencilFunc = ComparisonFunc::Never;
        };

        bool depthEnable = false;
        DepthWriteMask depthWriteMask = DepthWriteMask::Zero;
        ComparisonFunc depthFunc = ComparisonFunc::Never;
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        DepthStencilOp frontFace;
        DepthStencilOp backFace;
        bool depthBoundsTestEnable = false;
    };

    struct SwapchainDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bufferCount = 2;
        Format format = Format::BGRA8_UNORM;
        bool fullscreen = false;
        bool vsync = true;
        float clearColor[4] = { .4f, .4f, .4f, 1.0f };
    };

    struct PipelineStateDesc
    {
        const Shader* vs = nullptr;
        const Shader* gs = nullptr;
        const Shader* ps = nullptr;
        const RasterizerState* rs = nullptr;
        const DepthStencilState* dss = nullptr;
        const VertexInputLayout* il = nullptr;
        PrimitiveTopology pt = PrimitiveTopology::TriangleList;
    };

    struct SubresourceData
    {
        const void* mem = nullptr;      //!< Pointer to the beginning of the subresource data (pointer to beginning of resource + subresource offset).
        uint32_t rowPitch = 0;          //!< Bytes between two rows of a texture (2D and 3D textures).
        uint32_t slicePitch = 0;        //!< Bytes between two depth slices of a texture (3D textures only).

        static SubresourceData FromDesc(const void* data, const TextureDesc& desc)
        {
            const FormatInfo& formatInfo = GetFormatInfo(desc.format);
            SubresourceData subresource;
            subresource.mem = data;
            subresource.rowPitch = desc.width * formatInfo.bytesPerBlock;
            subresource.slicePitch = subresource.rowPitch * desc.height;
            return subresource;
        }
    };

    struct Rect
    {
        int32_t left = 0;
        int32_t top = 0;
        int32_t right = 0;
        int32_t bottom = 0;
    };

    //=============================================================
    //  Render Device Children
    //=============================================================

    struct RenderDeviceChild
    {
        std::shared_ptr<void> internal_state;
        inline bool IsValid() const { return internal_state.get() != nullptr; }
    };

    struct GPUResource : public RenderDeviceChild
    {
        enum class Type
        {
            Unknown,
            Buffer,
            Texture
        };

        Type type = Type::Unknown;
        void* mappedData = nullptr;
        uint32_t mappedSize = 0;

        constexpr bool IsTexture() const { return type == Type::Texture; }
        constexpr bool IsBuffer() const { return type == Type::Buffer; }
    };

    struct GPUBuffer : public GPUResource
    {
        GPUBufferDesc desc{};
        const GPUBufferDesc& GetDesc() const { return desc; }
    };

    struct GPUQuery : public GPUResource
    {
        GPUQueryDesc desc{};
    };

    struct Texture final : public GPUResource
    {
        TextureDesc desc{};
        const TextureDesc& GetDesc() const { return desc; }
    };

    struct RenderPassImage
    {
        enum class Type
        {
            RenderTarget,
            DepthStencil
        };

        enum class LoadOp
        {
            Load,
            Clear,
            DontCare
        };

        enum class StoreOp
        {
            Store,
            DontCare
        };

        enum class DepthResolveMode
        {
            Min,
            Max,
        };

        Type type = Type::RenderTarget;
        LoadOp loadOp = LoadOp::Load;
        StoreOp storeOp = StoreOp::Store;
        const Texture* texture = nullptr;
        ResourceStates prePassLayout = ResourceStates::Unknown;     // layout before the render pass
        ResourceStates layout = ResourceStates::Unknown;	        // layout within the render pass
        ResourceStates postPassLayout = ResourceStates::Unknown;	// layout after the render pass
        DepthResolveMode depthDesolveMode = DepthResolveMode::Min;

        static RenderPassImage RenderTarget(
            const Texture* resource,
            LoadOp loadOp = LoadOp::Load,
            StoreOp storeOp = StoreOp::Store,
            ResourceStates prePassLayout =  ResourceStates::Unknown,
            ResourceStates postPassLayout = ResourceStates::ShaderResourceBit)
        {
            RenderPassImage image;
            image.type = Type::RenderTarget;
            image.texture = resource;
            image.loadOp = loadOp;
            image.storeOp = storeOp;
            image.prePassLayout = prePassLayout;
            image.layout = ResourceStates::RenderTargetBit;
            image.postPassLayout = postPassLayout;
            return image;
        }

        static RenderPassImage DepthStencil(
            const Texture* resource,
            LoadOp loadOp = LoadOp::Load,
            StoreOp storeOp = StoreOp::Store,
            ResourceStates prePassLayout = ResourceStates::DepthWriteBit,
            ResourceStates layout = ResourceStates::DepthWriteBit,
            ResourceStates postPassLayout = ResourceStates::DepthWriteBit)
        {
            RenderPassImage image;
            image.type = Type::DepthStencil;
            image.texture = resource;
            image.loadOp = loadOp;
            image.storeOp = storeOp;
            image.prePassLayout = prePassLayout;
            image.layout = layout;
            image.postPassLayout = postPassLayout;
            return image;
        }
    };

    struct RenderPassInfo
    {
        Format rtFormats[8] = {};           // render target formats
        uint32_t rtCount = 0;               // number of render targets
        Format dsFormat = Format::Unknown;  // depth stencil format

        constexpr uint64_t GetHash() const
        {
            union Hasher
            {
                struct
                {
                    uint64_t rtFormat_0 : 6;
                    uint64_t rtFormat_1 : 6;
                    uint64_t rtFormat_2 : 6;
                    uint64_t rtFormat_3 : 6;
                    uint64_t rtFormat_4 : 6;
                    uint64_t rtFormat_5 : 6;
                    uint64_t rtFormat_6 : 6;
                    uint64_t rtFormat_7 : 6;
                    uint64_t dsFormat : 6;
                } bits;
                uint64_t value;
            };

            Hasher hasher = {};
            static_assert(sizeof(Hasher) == sizeof(uint64_t));
            hasher.bits.rtFormat_0 = (uint64_t)rtFormats[0];
            hasher.bits.rtFormat_1 = (uint64_t)rtFormats[1];
            hasher.bits.rtFormat_2 = (uint64_t)rtFormats[2];
            hasher.bits.rtFormat_3 = (uint64_t)rtFormats[3];
            hasher.bits.rtFormat_4 = (uint64_t)rtFormats[4];
            hasher.bits.rtFormat_5 = (uint64_t)rtFormats[5];
            hasher.bits.rtFormat_6 = (uint64_t)rtFormats[6];
            hasher.bits.rtFormat_7 = (uint64_t)rtFormats[7];
            hasher.bits.dsFormat = (uint64_t)dsFormat;
            return hasher.value;
        }

        static RenderPassInfo GetFrom(const RenderPassImage* images, uint32_t imageCount)
        {
            RenderPassInfo info;
            for (uint32_t i = 0; i < imageCount; ++i)
            {
                const RenderPassImage& image = images[i];
                const TextureDesc& desc = image.texture->GetDesc();
                switch (image.type)
                {
                case RenderPassImage::Type::RenderTarget:
                    info.rtFormats[info.rtCount++] = desc.format;
                    break;
                case RenderPassImage::Type::DepthStencil:
                    info.dsFormat = desc.format;
                    break;
                }
            }
            return info;
        }

        static RenderPassInfo GetFrom(const SwapchainDesc& swapchainDesc)
        {
            RenderPassInfo info{};
            info.rtCount = 1;
            info.rtFormats[0] = swapchainDesc.format;
            return info;
        }
    };

    struct Shader final : public RenderDeviceChild
    {
        ShaderType stage = ShaderType::Vertex;
        std::string code;
    };

    struct Sampler final : public RenderDeviceChild
    {
        SamplerDesc desc = {};
        const SamplerDesc& GetDesc() const { return desc; }
    };

    struct PipelineState final : public RenderDeviceChild
    {
        size_t hash{ 0 };
        PipelineStateDesc desc{};
        const PipelineStateDesc& GetDesc() const { return desc; }
    };

    struct Swapchain final : public RenderDeviceChild
    {
        SwapchainDesc desc;

        constexpr const SwapchainDesc& GetDesc() const { return desc; }
    };

    //=============================================================
    //  Render Device Interface Class
    //=============================================================

    struct CommandList
    {
        void* internal_state = nullptr;
        constexpr bool IsValid() const { return internal_state != nullptr; }
    };

    constexpr uint32_t DESCRIPTORBINDER_CBV_COUNT = 14;
    constexpr uint32_t DESCRIPTORBINDER_SRV_COUNT = 16;
    constexpr uint32_t DESCRIPTORBINDER_SAMPLER_COUNT = 8;

    struct DescriptorBindingTable
    {
        std::array<GPUBuffer, DESCRIPTORBINDER_CBV_COUNT>   CBV{};
        std::array<uint64_t, DESCRIPTORBINDER_CBV_COUNT>    CBV_offset{};
        std::array<GPUResource, DESCRIPTORBINDER_SRV_COUNT> SRV{};
        std::array<int, DESCRIPTORBINDER_SRV_COUNT>         SRV_index{};
        std::array<Sampler, DESCRIPTORBINDER_SAMPLER_COUNT> SAM{};
    };

    template <typename T>
    constexpr T AlignTo(T value, T alignment)
    {
        return ((value + alignment - T(1)) / alignment) * alignment;
    }

    class GraphicsDevice
    {
    protected:
        static constexpr uint32_t BUFFERCOUNT = 2;
        static constexpr bool VALIDATION_MODE_ENABLED = false;
        uint64_t frameCount = 0;
        uint64_t gpuTimestampFrequency = 0;

    public:
        virtual ~GraphicsDevice() = default;

        virtual bool CreateSwapchain(const SwapchainDesc* desc, WindowHandle window, Swapchain* swapchain) const = 0;
        virtual bool CreateBuffer(const GPUBufferDesc* desc, const void* initData, GPUBuffer* buffer) const = 0;
        virtual bool CreateQuery(const GPUQueryDesc* desc, GPUQuery* query) const = 0;
        virtual bool CreateTexture(const TextureDesc* desc, const SubresourceData* init_data, Texture* texture) const = 0;
        virtual bool CreateShader(ShaderType stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const = 0;
        virtual bool CreateSampler(const SamplerDesc* desc, Sampler* sampler) const = 0;
        virtual bool CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const = 0;

        virtual CommandList BeginCommandList(QueueType queue = QueueType::Graphics) = 0;
        virtual void ExecuteCommandLists() {}
        virtual void SetName(GPUResource* resource, const char* name) { (void)resource; (void)name; }
        virtual void SetName(Shader* shader, const char* name) { (void)shader; (void)name; }

        virtual void ClearPipelineStateCache() = 0;

        constexpr uint64_t GetFrameCount() const { return frameCount; }
        static constexpr uint32_t GetBufferCount() { return BUFFERCOUNT; }
        constexpr uint32_t GetBufferIndex() const { return GetFrameCount() % BUFFERCOUNT; }
        constexpr uint64_t GetTimestampFrequency() const { return gpuTimestampFrequency; }

        // Returns the minimum required alignment for buffer offsets when creating subresources
        virtual uint64_t GetMinOffsetAlignment(const GPUBufferDesc* desc) const = 0;

        struct MemoryUsage
        {
            uint64_t budget{ 0ull };    //!< Total video memory available for use by the current application (in bytes).
            uint64_t usage{ 0ull };     //!< Used video memory by the current application (in bytes).
        };

        // Returns video memory statistics for the current application
        virtual MemoryUsage GetMemoryUsage() const = 0;

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Command List functions are below:
        //	- These are used to record rendering commands to a CommandList
        //	- To get a CommandList that can be recorded into, call BeginCommandList()
        //	- These are not thread safe, only a single thread should use a single CommandList at one time

        virtual void BeginRenderPass(const Swapchain* swapchain, CommandList cmd) = 0;
        virtual void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd) = 0;
        virtual void EndRenderPass(CommandList cmd) = 0;

        virtual void BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd) = 0;
        virtual void BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd) = 0;
        virtual void BindPipelineState(const PipelineState* pso, CommandList cmd) = 0;
        virtual void BindVertexBuffers(const GPUBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd) = 0;
        virtual void BindIndexBuffer(const GPUBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd) = 0;
        virtual void BindStencilRef(uint32_t value, CommandList cmd) = 0;
        virtual void BindResource(const GPUResource* resource, int slot, CommandList cmd) = 0;
        virtual void BindSampler(const Sampler* sampler, uint32_t slot, CommandList cmd) = 0;
        virtual void BindConstantBuffer(const GPUBuffer* buffer, uint32_t slot, CommandList cmd, uint64_t offset = 0ull) = 0;

        virtual void CopyBuffer(const GPUBuffer* dst, uint64_t dst_offset, const GPUBuffer* src, uint64_t src_offset, uint64_t size, CommandList cmd) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t startVertexLocation, CommandList cmd) = 0;
        virtual void DrawIndexed(uint32_t index_count, uint32_t start_index_location, int32_t base_vertex_location, CommandList cmd) = 0;

        virtual void BeginQuery(const GPUQuery* query, uint32_t index, CommandList cmd) = 0;
        virtual void EndQuery(const GPUQuery* query, uint32_t index, CommandList cmd) = 0;
        virtual void ResolveQuery(const GPUQuery* query, uint32_t index, uint32_t count, const GPUBuffer* dest, uint64_t destOffset, CommandList cmd) = 0;
        virtual void ResetQuery(const GPUQuery* query, uint32_t index, uint32_t count, CommandList cmd) = 0;

        virtual void PushConstants(const void* data, uint32_t size, CommandList cmd, uint32_t offset = 0) = 0;

        virtual void BeginEvent(const char* name, CommandList cmd) = 0;
        virtual void EndEvent(CommandList cmd) = 0;

        // Some helpers:
        struct GPULinearAllocator
        {
            GPUBuffer buffer;
            uint64_t offset = 0ull;
            uint64_t alignment = 0ull;
            void Reset()
            {
                offset = 0ull;
            }
        };
        virtual GPULinearAllocator& GetFrameAllocator(CommandList cmd) = 0;

        struct GPUAllocation
        {
            void* data = nullptr;	// application can write to this. Reads might be not supported or slow. The offset is already applied
            GPUBuffer buffer;		// application can bind it to the GPU
            uint64_t offset = 0;	// allocation's offset from the GPUbuffer's beginning

            // Returns true if the allocation was successful
            inline bool IsValid() const { return data != nullptr && buffer.IsValid(); }
        };

        // Allocates temporary memory that the CPU can write and GPU can read. 
        // Allocation is only alive for one frame and automatically invalidated after that.
        [[nodiscard]] GPUAllocation AllocateGPU(uint64_t dataSize, CommandList cmd)
        {
            GPUAllocation allocation;
            if (dataSize == 0)
                return allocation;

            GPULinearAllocator& allocator = GetFrameAllocator(cmd);

            const uint64_t free_space = allocator.buffer.desc.size - allocator.offset;
            if (dataSize > free_space)
            {
                GPUBufferDesc desc;
                desc.cpuAccess = CpuAccessMode::Read;
                desc.usage = BufferUsage::ConstantBufferBit | BufferUsage::VertexBufferBit | BufferUsage::IndexBufferBit;
                allocator.alignment = GetMinOffsetAlignment(&desc);
                desc.size = AlignTo((allocator.buffer.desc.size + dataSize) * 2, allocator.alignment);
                CreateBuffer(&desc, nullptr, &allocator.buffer);
                SetName(&allocator.buffer, "FrameAllocationBuffer");
                allocator.offset = 0;

                CYB_TRACE("Increasing GPU frame allocation for cmd(0x{:x}) bufferIndex {} to {:.1f}kb", (ptrdiff_t)cmd.internal_state, GetBufferIndex(), desc.size / 1024.0f);
            }

            allocation.buffer = allocator.buffer;
            allocation.offset = allocator.offset;
            allocation.data = (void*)((size_t)allocator.buffer.mappedData + allocator.offset);

            allocator.offset += AlignTo(dataSize, allocator.alignment);

            assert(allocation.IsValid());
            return allocation;
        }

        // Update a CpuAccessMode::Default buffer data
        // Since it uses a GPU Copy operation, appropriate synchronization is expected
        // And it cannot be used inside a RenderPass
        void UpdateBuffer(const GPUBuffer* buffer, const void* data, CommandList cmd, uint64_t size = ~0, uint64_t offset = 0)
        {
            if (buffer == nullptr || data == nullptr)
                return;

            size = Min(buffer->desc.size, size);
            if (size == 0)
                return;
            GPUAllocation allocation = AllocateGPU(size, cmd);
            std::memcpy(allocation.data, data, size);
            CopyBuffer(buffer, offset, &allocation.buffer, allocation.offset, size, cmd);
        }

        // Bind a constant buffer with data for a specific command list
        // This will be done on the CPU to an UPLOAD buffer, so this can be used inside a RenderPass
        // But this will be only visible on the command list it was bound to
        template<typename T>
        void BindDynamicConstantBuffer(const T& data, uint32_t slot, CommandList cmd)
        {
            GPUAllocation allocation = AllocateGPU(sizeof(T), cmd);
            std::memcpy(allocation.data, &data, sizeof(T));
            BindConstantBuffer(&allocation.buffer, slot, cmd, allocation.offset);
        }
    };

    inline const FormatInfo& GetFormatInfo(Format format)
    {
        static constexpr FormatInfo s_formatInfo[] = {
            { Format::Unknown,      "UNKNOWN",      0,  0,  false,  false   },
            { Format::R8_UNORM,     "R8_UNORM",     1,  1,  false,  false   },
            { Format::RGBA8_UINT,   "RGBA8_UINT",   4,  1,  false,  false   },
            { Format::RGBA8_UNORM,  "RGBA8_UNORM",  4,  1,  false,  false   },
            { Format::BGRA8_UNORM,  "BGRA8_UNORM",  4,  1,  false,  false   },
            { Format::R16_FLOAT,    "R16_FLOAT",    2,  1,  false,  false   },
            { Format::RG16_FLOAT,   "RG16_FLOAT",   4,  1,  false,  false   },
            { Format::R32_FLOAT,    "R32_FLOAT",    4,  1,  false,  false   },
            { Format::RG32_FLOAT,   "RG32_FLOAT",   8,  1,  false,  false   },
            { Format::RGB32_FLOAT,  "RGB32_FLOAT",  12, 1,  false,  false   },
            { Format::RGBA32_FLOAT, "RGBA32_FLOAT", 16, 1,  false,  false   },
            { Format::D24S8,        "D24S8",        4,  1,  true,   true    },
            { Format::D32,          "D32",          4,  1,  true,   false   },
            { Format::D32S8,        "D32S8",        8,  1,  true,   true    }
        };

        static_assert(sizeof(s_formatInfo) / sizeof(FormatInfo) == (size_t)Format::COUNT);

        if ((uint32_t)format >= (uint32_t)Format::COUNT)
            return s_formatInfo[0]; // UNKNOWN

        const FormatInfo& info = s_formatInfo[(uint32_t)format];
        assert(info.format == format);
        return info;
    }

    inline GraphicsDevice*& GetDevice()
    {
        static GraphicsDevice* device = nullptr;
        return device;
    }
}

