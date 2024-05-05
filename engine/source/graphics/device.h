#pragma once
#include <vector>
#include "core/platform.h"
#include "core/mathlib.h"
#include "core/logger.h"

namespace cyb::graphics
{
    struct Texture;
    struct Shader;

    enum class BindFlags
    {
        None = 0,
        VertexBufferBit = (1 << 0),
        IndexBufferBit = (1 << 1),
        ConstantBufferBit = (1 << 2),
        RenderTargetBit = (1 << 3),
        DepthStencilBit = (1 << 4),
        ShaderResourceBit = (1 << 5)
    };
    CYB_ENABLE_BITMASK_OPERATORS(BindFlags);

    enum class ResourceMiscFlag
    {
        None = 0,
        BufferRawBit = (1 << 0),
        BufferStructuredBit = (1 << 1),
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceMiscFlag);

    enum class MemoryAccess
    {
        Default,                            // CPU no access, GPU read/write
        Upload,                             // CPU write, GPU read
        Readback                            // CPU read, GPU write
    };

    enum class TextureFilter
    {
        Point,
        Bilinear,
        Trilinear,
        Anisotropic
    };

    enum class TextureAddressMode
    {
        Clamp,
        Wrap,
        Mirror,
        Border
    };

    enum class TextureComponentSwizzle
    {
        Identity,
        Zero,
        One,
        R,
        G,
        B,
        A
    };

    enum class Format
    {
        Unknown,
        R32G32B32A32_Float,                 // Four-component, 128-bit floating-point format with 32-bit channels
        R8G8B8A8_Uint,                      // Four-component, 32-bit unsigned-integer format with 8-bit channels
        R8G8B8A8_Unorm,                     // Four-component, 32-bit unsigned-normalized integer format with 8-bit channels
        R16G16_Float,                       // Two-component, 32-bit floating-point format with 16-bit channels 
        R32G32_Float,                       // Two-component, 64-bit floating-point format with 32-bit channels
        R8_Unorm,                           // Single-component, 8-bit unsigned-normalized integer swizzeled to { r, r, r, 1 }
        R32_Float,                          // Single-component, 32-bit floating-point format swizzeled to { r, r, r, 1 }
        R16_Float,                          // Single-component, 16-bit floating-point format swizzeled to { r, r, r, 1 }
        D32_Float,                          // Single-component, 32-bit floating-point format for depth
        D24_Float_S8_Uint,                  // Two-component, Depth (24-bit) + stencil (8-bit)
        D32_Float_S8_Uint,                  // Two-component, Depth (32-bit) + stencil (8-bit) (24-bits unused)
        B8G8R8A8_Unorm,
        R32G32B32_Float
    };

    enum class IndexBufferFormat
    {
        Uint16,
        Uint32
    };

    enum class SubresourceType
    {
        SRV,                                // Shader resource view
        RTV,                                // Render target view
        DSV                                 // Depth stencil view
    };

    enum class PolygonMode
    {
        Fill,
        Line,
        Point
    };

    enum class CullMode
    {
        None,
        Front,
        Back
    };

    enum class FrontFace
    {
        CCW,                                // Counterclockwise
        CW                                  // Clockwise
    };

    enum class PrimitiveTopology
    {
        TriangleList,
        TriangleStrip,
        PointList,
        LineList,
        LineStrip
    };

    enum class ComparisonFunc
    {
        Never,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Allways
    };

    enum class DepthWriteMask
    {
        Zero,	                            // Disables depth write
        All,	                            // Enables depth write
    };

    enum class StencilOp
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

    enum class ShaderStage
    {
        VS,                                 // Vertex shader
        FS,                                 // Fragment shader
        GS,                                 // Geometry shader
        _Count
    };

    enum class ShaderFormat
    {
        None,
        SpirV,
        GLSL
    };

    enum class QueueType
    {
        Graphics,
        Compute,
        Copy,
        _Count
    };

    enum class ResourceState
    {
        // Common resource states:
        Undefined = 0,                      // Invalid state (doesen't preserve contents)
        ShaderResourceBit = 1 << 0,         // Shader resource, read only
        ShaderResourceComputeBit = 1 << 1,  // Shader resource, read only, non-pixel shader
        UnorderedAccessBit = 1 << 2,        // Shader resource, write enabled
        CopySrcBit = 1 << 3,                // Copy from
        CopyDstBit = 1 << 4,                // Copy to

        // Texture specific resource states:
        RenderTargetBit = 1 << 5,	        // Render target, write enabled
        DepthStencilBit = 1 << 6,	        // Depth stencil, write enabled
        DepthStencil_ReadOnlyBit = 1 << 7,  // Depth stencil, read only

        // GPUBuffer specific resource states:
        VertexBufferBit = 1 << 9,           // Vertex buffer, read only
        IndexBufferBit = 1 << 10,           // Index buffer, read only
        ConstantBufferBit = 1 << 11,        // Constant buffer, read only
        IndirectArgumentBit = 1 << 12,      // Argument buffer to DrawIndirect() or DispatchIndirect()
        RaytracingAccelerationStructureBit = 1 << 13, // Acceleration structure storage or scratch
        PredictionBit = 1 << 14             // Storage for predication comparison value
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceState);

    enum class GPUQueryType
    {
        Timestamp,			                // Retrieve time point of gpu execution
        Occlusion,			                // How many samples passed depth test?
        OcclusionBinary 	                // Depth test passed or not?
    };

    struct GPUBufferDesc
    {
        uint64_t size = 0;
        MemoryAccess usage = MemoryAccess::Default;
        BindFlags bindFlags = BindFlags::None;
        ResourceMiscFlag miscFlags = ResourceMiscFlag::None;
        uint32_t stride = 0;                // Needed for struct buffer types
    };

    struct GPUQueryDesc
    {
        GPUQueryType type = GPUQueryType::Timestamp;
        uint32_t queryCount = 0;
    };

    struct Viewport
    {
        float x = 0;                        // Top-Left
        float y = 0;                        // Top-Left
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
        TextureFilter filter = TextureFilter::Point;
        TextureAddressMode addressU = TextureAddressMode::Wrap;
        TextureAddressMode addressV = TextureAddressMode::Wrap;
        TextureAddressMode addressW = TextureAddressMode::Wrap;
        float lodBias = 0.0f;
        float maxAnisotropy = 16.0f;
        XMFLOAT4 borderColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float minLOD = 0.0f;
        float maxLOD = FLT_MAX;
    };

    struct TextureComponentMapping
    {
        TextureComponentSwizzle r = TextureComponentSwizzle::Identity;
        TextureComponentSwizzle g = TextureComponentSwizzle::Identity;
        TextureComponentSwizzle b = TextureComponentSwizzle::Identity;
        TextureComponentSwizzle a = TextureComponentSwizzle::Identity;
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

    struct TextureDesc
    {
        enum class Type
        {
            Texture1D,          // UNTESTED
            Texture2D,
            Texture3D           // NOT IMPLEMENTED
        };

        Type type = Type::Texture2D;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arraySize = 1;
        Format format = Format::Unknown;
        TextureComponentMapping components;
        uint32_t mipLevels = 1;
        BindFlags bindFlags = BindFlags::None;
        ClearValue clear = {};
        ResourceState layout = ResourceState::ShaderResourceBit;
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

    struct SwapChainDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bufferCount = 2;
        Format format = Format::B8G8R8A8_Unorm;
        bool fullscreen = false;
        bool vsync = true;
        float clearColor[4] = { .4f, .4f, .4f, 1.0f };
    };

    struct PipelineStateDesc
    {
        const Shader* vs = nullptr;
        const Shader* gs = nullptr;
        const Shader* fs = nullptr;
        const RasterizerState* rs = nullptr;
        const DepthStencilState* dss = nullptr;
        const VertexInputLayout* il = nullptr;
        PrimitiveTopology pt = PrimitiveTopology::TriangleList;
    };

    struct SubresourceData
    {
        const void* mem = nullptr;	        // Pointer to the beginning of the subresource data (pointer to beginning of resource + subresource offset)
        uint32_t rowPitch = 0;			    // Bytes between two rows of a texture (2D and 3D textures)
        uint32_t slicePitch = 0;		    // Bytes between two depth slices of a texture (3D textures only)
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
        uint32_t mappedRowPitch = 0;

        constexpr bool IsTexture() const { return type == Type::Texture; }
        constexpr bool IsBuffer() const { return type == Type::Buffer; }
    };

    struct GPUBuffer : public GPUResource
    {
        GPUBufferDesc desc;
        const GPUBufferDesc& GetDesc() const { return desc; }
    };

    struct GPUQuery : public GPUResource
    {
        GPUQueryDesc desc;
    };

    struct Texture final : public GPUResource
    {
        TextureDesc desc;
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
        ResourceState prePassLayout = ResourceState::Undefined;	    // layout before the render pass
        ResourceState layout = ResourceState::Undefined;	        // layout within the render pass
        ResourceState postPassLayout = ResourceState::Undefined;	// layout after the render pass
        DepthResolveMode depthDesolveMode = DepthResolveMode::Min;

        static RenderPassImage RenderTarget(
            const Texture* resource,
            LoadOp loadOp = LoadOp::Load,
            StoreOp storeOp = StoreOp::Store,
            ResourceState prePassLayout = ResourceState::ShaderResourceBit,
            ResourceState postPassLayout = ResourceState::ShaderResourceBit)
        {
            RenderPassImage image;
            image.type = Type::RenderTarget;
            image.texture = resource;
            image.loadOp = loadOp;
            image.storeOp = storeOp;
            image.prePassLayout = prePassLayout;
            image.layout = ResourceState::RenderTargetBit;
            image.postPassLayout = postPassLayout;
            return image;
        }

        static RenderPassImage DepthStencil(
            const Texture* resource,
            LoadOp loadOp = LoadOp::Load,
            StoreOp storeOp = StoreOp::Store,
            ResourceState prePassLayout = ResourceState::DepthStencilBit,
            ResourceState layout = ResourceState::DepthStencilBit,
            ResourceState postPassLayout = ResourceState::DepthStencilBit)
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
        Format rtFormats[8];                // render target formats
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
                    uint64_t dsFormat   : 6;
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

        static const RenderPassInfo GetFrom(const RenderPassImage* images, uint32_t imageCount)
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

        static const RenderPassInfo GetFrom(const SwapChainDesc& swapchainDesc)
        {
            RenderPassInfo info;
            info.rtCount = 1;
            info.rtFormats[0] = swapchainDesc.format;
            return info;
        }
    };

    struct Shader final : public RenderDeviceChild
    {
        ShaderStage stage = ShaderStage::VS;
        std::string code;
    };

    struct Sampler final : public RenderDeviceChild
    {
        SamplerDesc desc = {};
        const SamplerDesc& GetDesc() const { return desc; }
    };

    struct PipelineState final : public RenderDeviceChild
    {
        size_t hash = 0;
        PipelineStateDesc desc;
        const PipelineStateDesc& GetDesc() const { return desc; }
    };

    struct SwapChain final : public RenderDeviceChild
    {
        SwapChainDesc desc;

        constexpr const SwapChainDesc& GetDesc() const { return desc; }
    };

    //=============================================================
    //  Render Device Interface Class
    //=============================================================

    struct FrameStats
    {
        uint32_t drawCalls = 0;
        uint64_t triangleCount = 0;
    };

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
        GPUBuffer CBV[DESCRIPTORBINDER_CBV_COUNT];
        uint64_t CBV_offset[DESCRIPTORBINDER_CBV_COUNT] = {};
        GPUResource SRV[DESCRIPTORBINDER_SRV_COUNT];
        int SRV_index[DESCRIPTORBINDER_SRV_COUNT] = {};
        Sampler SAM[DESCRIPTORBINDER_SAMPLER_COUNT];
    };

    template <typename T>
    constexpr T AlignTo(T value, T alignment)
    {
        return ((value + alignment - T(1)) / alignment) * alignment;
    }

    class GraphicsDevice
    {
    protected:
        static const uint32_t BUFFERCOUNT = 2;
        const bool VALIDATION_MODE_ENABLED = true;
        uint64_t frameCount = 0;
        uint64_t gpuTimestampFrequency = 0;

    public:
        virtual ~GraphicsDevice() = default;

        virtual bool CreateSwapChain(const SwapChainDesc* desc, WindowHandle window, SwapChain* swapchain) const = 0;
        virtual bool CreateBuffer(const GPUBufferDesc* desc, const void* initData, GPUBuffer* buffer) const = 0;
        virtual bool CreateQuery(const GPUQueryDesc* desc, GPUQuery* query) const = 0;
        virtual bool CreateTexture(const TextureDesc* desc, const SubresourceData* init_data, Texture* texture) const = 0;
        virtual bool CreateShader(ShaderStage stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const = 0;
        virtual bool CreateSampler(const SamplerDesc* desc, Sampler* sampler) const = 0;
        virtual bool CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const = 0;

        virtual CommandList BeginCommandList(QueueType queue = QueueType::Graphics) = 0;
        virtual void SubmitCommandList() {}
        virtual void SetName(GPUResource* resource, const char* name) { }
        virtual void SetName(Shader* shader, const char* name) { }

        virtual void ClearPipelineStateCache() = 0;

        constexpr uint64_t GetFrameCount() const { return frameCount; }
        static constexpr uint32_t GetBufferCount() { return BUFFERCOUNT; }
        constexpr uint32_t GetBufferIndex() const { return GetFrameCount() % BUFFERCOUNT; }
        constexpr uint64_t GetTimestampFrequency() const { return gpuTimestampFrequency; }

        // Returns the minimum required alignment for buffer offsets when creating subresources
        virtual uint64_t GetMinOffsetAlignment(const GPUBufferDesc* desc) const = 0;

        struct MemoryUsage
        {
            uint64_t budget = 0ull;		// Total video memory available for use by the current application (in bytes)
            uint64_t usage = 0ull;		// Used video memory by the current application (in bytes)
        };

        // Returns video memory statistics for the current application
        virtual MemoryUsage GetMemoryUsage() const = 0;

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Command List functions are below:
        //	- These are used to record rendering commands to a CommandList
        //	- To get a CommandList that can be recorded into, call BeginCommandList()
        //	- These are not thread safe, only a single thread should use a single CommandList at one time

        virtual void BeginRenderPass(const SwapChain* swapchain, CommandList cmd) = 0;
        virtual void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd) = 0;
        virtual void EndRenderPass(CommandList cmd) = 0;

        virtual void BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd) = 0;
        virtual void BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd) = 0;
        virtual void BindPipelineState(const PipelineState* pso, CommandList cmd) = 0;
        virtual void BindVertexBuffers(const GPUBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd) = 0;
        virtual void BindIndexBuffer(const GPUBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd) = 0;
        virtual void BindStencilRef(uint32_t value, CommandList cmd) = 0;;
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
                desc.usage = MemoryAccess::Upload;
                desc.bindFlags = BindFlags::ConstantBufferBit | BindFlags::VertexBufferBit | BindFlags::IndexBufferBit | BindFlags::ShaderResourceBit;
                desc.miscFlags = ResourceMiscFlag::BufferRawBit;
                allocator.alignment = GetMinOffsetAlignment(&desc);
                desc.size = AlignTo((allocator.buffer.desc.size + dataSize) * 2, allocator.alignment);
                CreateBuffer(&desc, nullptr, &allocator.buffer);
                SetName(&allocator.buffer, "FrameAllocationBuffer");
                allocator.offset = 0;
                
                CYB_TRACE("Increasing GPU frame allocation buffer to {:.1f}kb (commandList=0x{:0x}, bufferIndex={})", desc.size / 1024.0f, (ptrdiff_t)cmd.internal_state, GetBufferIndex());
            }

            allocation.buffer = allocator.buffer;
            allocation.offset = allocator.offset;
            allocation.data = (void*)((size_t)allocator.buffer.mappedData + allocator.offset);

            allocator.offset += AlignTo(dataSize, allocator.alignment);

            assert(allocation.IsValid());
            return allocation;
        }

        // Update a MemoryAccess::Default buffer data
        // Since it uses a GPU Copy operation, appropriate synchronization is expected
        // And it cannot be used inside a RenderPass
        void UpdateBuffer(const GPUBuffer* buffer, const void* data, CommandList cmd, uint64_t size = ~0, uint64_t offset = 0)
        {
            if (buffer == nullptr || data == nullptr)
                return;
     
            size = std::min(buffer->desc.size, size);
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

    constexpr bool IsFormatDepthSupport(Format format)
    {
        switch (format)
        {
        case Format::D32_Float:
        case Format::D24_Float_S8_Uint:
        case Format::D32_Float_S8_Uint:
            return true;

        case Format::Unknown:
        case Format::R32G32B32A32_Float:
        case Format::R8G8B8A8_Uint:
        case Format::R8G8B8A8_Unorm:
        case Format::R16G16_Float:
        case Format::R32G32_Float:
        case Format::R8_Unorm:
        case Format::R32_Float:
        case Format::R16_Float:
        case Format::B8G8R8A8_Unorm:
        case Format::R32G32B32_Float:
            return false;
        }

        assert(0);
        return false;
    }

    constexpr bool IsFormatStencilSupport(Format format)
    {
        switch (format)
        {
        case Format::D24_Float_S8_Uint:
        case Format::D32_Float_S8_Uint:
            return true;

        case Format::Unknown:
        case Format::D32_Float:
        case Format::R32G32B32A32_Float:
        case Format::R8G8B8A8_Uint:
        case Format::R8G8B8A8_Unorm:
        case Format::R16G16_Float:
        case Format::R32G32_Float:
        case Format::R8_Unorm:
        case Format::R32_Float:
        case Format::R16_Float:
        case Format::B8G8R8A8_Unorm:
        case Format::R32G32B32_Float:
            return false;
        }

        assert(0);
        return false;
    }

    constexpr uint32_t GetFormatStride(Format format)
    {
        switch (format)
        {
        case Format::R32G32B32A32_Float:
            return 16u;

        case Format::R32G32B32_Float:
            return 12u;

        case Format::R32G32_Float:
        case Format::D32_Float_S8_Uint:
            return 8u;

        case Format::R8G8B8A8_Uint:
        case Format::R8G8B8A8_Unorm:
        case Format::R16G16_Float:
        case Format::R32_Float:
        case Format::D32_Float:
        case Format::D24_Float_S8_Uint:
        case Format::B8G8R8A8_Unorm:
            return 4u;

        case Format::R16_Float:
            return 2u;

        case Format::R8_Unorm:
            return 1u;

        case Format::Unknown:
        default:
            break;
        }

        assert(0);
        return 16u;
    }

    inline graphics::GraphicsDevice*& GetDevice()
    {
        static graphics::GraphicsDevice* device = nullptr;
        return device;
    }
}

