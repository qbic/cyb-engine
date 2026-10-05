#pragma once
#include "core/enum_flags.h"
#include "core/mathlib.h"
#include "core/ref_count.h"
#include "core/sys.h"
#include "core/logger.h"
#include "graphics/display.h"   // for WindowHandle
#include <array>

template <typename To, typename From>
    requires std::is_pointer_v<To>
[[nodiscard]] inline To check_cast(From* ptr) noexcept
{
#ifdef _DEBUG
    static_assert(!std::is_same_v<To, From>, "redundant cast");
    To checked = dynamic_cast<To>(ptr);
    assert(checked != nullptr && "invalid cast");
    return checked;
#else
    return static_cast<To>(ptr);
#endif
}

namespace cyb::rhi
{
    struct ITexture;
    struct IShader;

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
        Read,                           //!< CPU read, GPU write
        Write                           //!< CPU write, GPU read
    };

    enum class Filtering : uint8_t
    {
        None                = 0,
        Min                 = BIT(0),   //!< Minification filtering
        Mag                 = BIT(1),   //!< Magnification filtering
        Mip                 = BIT(2),   //!< Mipmap filtering
    };
    CYB_ENABLE_BITMASK_OPERATORS(Filtering);

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
        std::string_view name;
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

    enum class CommandQueue : uint8_t
    {
        Graphics,
        Compute,
        Transfer,
        Count
    };

    enum class ResourceStates : uint32_t
    {
        // Common resource states
        Unknown             = 0,        //!< Dont preserve contents
        ShaderResourceBit   = BIT(0),   //!< Shader resource, read only
        UnorderedAccessBit  = BIT(1),   //!< Shader resource, write enabled
        CopySourceBit       = BIT(2),   //!< Copy from
        CopyDestBit         = BIT(3),   //!< Copy to

        // Texture specific resource states
        RenderTargetBit     = BIT(10),  //!< Render target, write enabled
        DepthWriteBit       = BIT(11),  //!< Depth stencil, write enabled
        DepthReadBit        = BIT(12),  //!< Depth stencil, read only

        // GPUBuffer specific resource states
        VertexBufferBit     = BIT(20),  //!< Vertex buffer, read only
        IndexBufferBit      = BIT(21),  //!< Index buffer, read only
        ConstantBufferBit   = BIT(22),  //!< Constant buffer, read only
        IndirectArgumentBit = BIT(23),  //!< Argument buffer to DrawIndirect() or DispatchIndirect()
        AccelStructBit      = BIT(24),  //!< Acceleration structure storage or scratch
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceStates);

    enum class QueryType : uint8_t
    {
        Timestamp,                      //!< Retrieve time point of gpu execution
        Occlusion,                      //!< How many samples passed depth test?
        OcclusionBinary                 //!< Depth test passed or not?
    };

    struct BufferDesc
    {
        uint64_t size = 0;
        CpuAccessMode cpuAccess = CpuAccessMode::None;
        BufferUsage usage = BufferUsage::None;
        uint32_t stride = 0;            // Needed for struct buffer types
		std::string debugName{};
    };

    struct QueryDesc
    {
        QueryType type = QueryType::Timestamp;
        uint32_t queryCount = 0;
    };

    struct Viewport
    {
        float x = 0;                    // top-left
        float y = 0;                    // top-left
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
            std::string_view inputName;
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
        Filtering filter = Filtering::Min | Filtering::Mag | Filtering::Mip;
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
            float depth = 1.0f;
            uint32_t stencil = 0;
        };

        std::array<float, 4> color = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
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
        ClearValue clear;
        ResourceStates initialState = ResourceStates::ShaderResourceBit;
        std::string debugName{};
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
        std::array<float, 4> clearColor = {{ .4f, .4f, .4f, 1.0f }};
    };

	struct ShaderDesc
	{
		ShaderType stage = ShaderType::Count;
		ShaderFormat format = ShaderFormat::None;
		const void* bytecode = nullptr;
		size_t bytecodeLength = 0;
		std::string debugName{};
	};

    struct PipelineStateDesc
    {
        const IShader* vs = nullptr;
        const IShader* gs = nullptr;
        const IShader* ps = nullptr;
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

	struct IBuffer : public IResource
    {
        [[nodiscard]] virtual const BufferDesc& GetDesc() const = 0;
        [[nodiscard]] virtual void* MappedMemory() = 0;
    };
	using BufferHandle = RefCountPtr<IBuffer>;

    struct IQuery : public IResource
    {
        [[nodiscard]] virtual const QueryDesc& GetDesc() const = 0;
    };
    using QueryHandle = RefCountPtr<IQuery>;

	struct IEventQuery : public IResource
	{
	};
	using EventQueryHandle = RefCountPtr<IEventQuery>;

    struct ITexture : public IResource
    {
        [[nodiscard]] virtual const TextureDesc& GetDesc() const = 0;
    };
	using TextureHandle = RefCountPtr<ITexture>;

	struct IShader : public IResource
	{
		[[nodiscard]] virtual const ShaderDesc& GetDesc() const = 0;
	};
	using ShaderHandle = RefCountPtr<IShader>;

    struct ISampler : public IResource
    {
		[[nodiscard]] virtual const SamplerDesc& GetDesc() const = 0;
    };
	using SamplerHandle = RefCountPtr<ISampler>;

    struct IPipelineState : public IResource
    {
        [[nodiscard]] virtual const PipelineStateDesc& GetDesc() const = 0;
    };
	using PipelineStateHandle = RefCountPtr<IPipelineState>;

    struct ISwapchain : public IResource
    {
        [[nodiscard]] virtual const SwapchainDesc& GetDesc() const = 0;
		[[nodiscard]] virtual bool ResizeBuffers(const SwapchainDesc& desc) = 0;
		virtual void Present() = 0;
    };
	using SwapchainHandle = RefCountPtr<ISwapchain>;

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
        const ITexture* texture = nullptr;
        ResourceStates prePassLayout = ResourceStates::Unknown;     // layout before the render pass
        ResourceStates layout = ResourceStates::Unknown;	        // layout within the render pass
        ResourceStates postPassLayout = ResourceStates::Unknown;	// layout after the render pass
        DepthResolveMode depthDesolveMode = DepthResolveMode::Min;

        static RenderPassImage RenderTarget(
            const ITexture* resource,
            LoadOp loadOp = LoadOp::Load,
            StoreOp storeOp = StoreOp::Store,
            ResourceStates prePassLayout = ResourceStates::Unknown,
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
            const ITexture* resource,
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
        std::array<Format, 8> rtFormats{};  // render target formats
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

            Hasher hasher{};
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
            RenderPassInfo info{};
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
        std::array<const IBuffer*, DESCRIPTORBINDER_CBV_COUNT> CBV{};
        std::array<uint64_t, DESCRIPTORBINDER_CBV_COUNT> CBV_offset{};
        std::array<IResource*, DESCRIPTORBINDER_SRV_COUNT> SRV{};
        std::array<int, DESCRIPTORBINDER_SRV_COUNT> SRV_index{};
        std::array<const ISampler*, DESCRIPTORBINDER_SAMPLER_COUNT> SAM{};
    };

    class GraphicsDevice
    {
    protected:
        static constexpr uint32_t BUFFERCOUNT = 2;
        static constexpr bool VALIDATION_MODE_ENABLED = false;
        uint64_t frameCount = 0;
        uint64_t gpuTimestampFrequency = 0;

    public:
        virtual ~GraphicsDevice() = default;

        virtual SwapchainHandle CreateSwapchain(const SwapchainDesc* desc, NativeWindowHandle window) const = 0;
        virtual BufferHandle CreateBuffer(const BufferDesc* desc, const void* initData) const = 0;
        virtual QueryHandle CreateQuery(const QueryDesc* desc) const = 0;
		virtual EventQueryHandle CreateEventQuery() const = 0;
        virtual TextureHandle CreateTexture(const TextureDesc* desc, const SubresourceData* init_data) const = 0;
        virtual ShaderHandle CreateShader(const ShaderDesc* desc) const = 0;
        virtual SamplerHandle CreateSampler(const SamplerDesc* desc) const = 0;
        virtual PipelineStateHandle CreatePipelineState(const PipelineStateDesc* desc) const = 0;

        virtual CommandList BeginCommandList(CommandQueue queue = CommandQueue::Graphics) = 0;
        virtual void ExecuteCommandLists() {}

        /**
         * @brief Make the CPU wait until all submitted GPU work is finished execution.
         */
        virtual void WaitForGPU() const = 0;

        virtual void ClearPipelineStateCache() = 0;

        constexpr uint64_t GetFrameCount() const { return frameCount; }
        static constexpr uint32_t GetBufferCount() { return BUFFERCOUNT; }
        constexpr uint32_t GetBufferIndex() const { return GetFrameCount() % BUFFERCOUNT; }
        constexpr uint64_t GetTimestampFrequency() const { return gpuTimestampFrequency; }

        // Returns the minimum required alignment for buffer offsets when creating subresources
        virtual uint64_t GetMinOffsetAlignment(const BufferDesc* desc) const = 0;

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

        virtual void BeginRenderPass(ISwapchain* swapchain, CommandList cmd) = 0;
        virtual void BeginRenderPass(const RenderPassImage* images, uint32_t imageCount, CommandList cmd) = 0;
        virtual void EndRenderPass(CommandList cmd) = 0;

        virtual void BindScissorRects(const Rect* rects, uint32_t rectCount, CommandList cmd) = 0;
        virtual void BindViewports(const Viewport* viewports, uint32_t viewportCount, CommandList cmd) = 0;
        virtual void BindPipelineState(const IPipelineState* pso, CommandList cmd) = 0;
        virtual void BindVertexBuffers(const IBuffer* const* vertexBuffers, uint32_t count, const uint32_t* strides, const uint64_t* offsets, CommandList cmd) = 0;
        virtual void BindIndexBuffer(const IBuffer* index_buffer, const IndexBufferFormat format, uint64_t offset, CommandList cmd) = 0;
        virtual void BindStencilRef(uint32_t value, CommandList cmd) = 0;
        virtual void BindResource(const IResource* resource, int slot, CommandList cmd) = 0;
        virtual void BindSampler(const ISampler* sampler, uint32_t slot, CommandList cmd) = 0;
        virtual void BindConstantBuffer(const IBuffer* buffer, uint32_t slot, CommandList cmd, uint64_t offset = 0ull) = 0;

        virtual void CopyBuffer(const IBuffer* dst, uint64_t dst_offset, const IBuffer* src, uint64_t src_offset, uint64_t size, CommandList cmd) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t startVertexLocation, CommandList cmd) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation, CommandList cmd) = 0;

        virtual void BeginQuery(IQuery* query, uint32_t index, CommandList cmd) = 0;
        virtual void EndQuery(IQuery* query, uint32_t index, CommandList cmd) = 0;
        virtual void ResolveQuery(const IQuery* query, uint32_t index, uint32_t count, IBuffer* dest, uint64_t destOffset, CommandList cmd) = 0;
        virtual void ResetQuery(IQuery* query, uint32_t index, uint32_t count, CommandList cmd) = 0;

        virtual void SetEventQuery(IEventQuery* query, CommandQueue queue) = 0;
		virtual bool PollEventQuery(IEventQuery* query) = 0;
		virtual void WaitEventQuery(IEventQuery* query) = 0;
		virtual void ResetEventQuery(IEventQuery* query) = 0;

        virtual void PushConstants(const void* data, uint32_t size, CommandList cmd, uint32_t offset = 0) = 0;

        virtual void BeginEvent(const char* name, CommandList cmd) = 0;
        virtual void EndEvent(CommandList cmd) = 0;

        struct GPULinearAllocator
        {
            BufferHandle buffer;
            uint64_t offset = 0;
            uint64_t alignment = 0;

            void Reset()
            {
                offset = 0u;
            }
        };
        virtual GPULinearAllocator& GetFrameAllocator(CommandList cmd) = 0;

        struct ScratchBuffer
        {
			void* data = nullptr;	   // CPU pointer (offset allready applied)
            IBuffer* buffer = nullptr; // handle for GPU binding
			uint64_t offset = 0;	   // offset from buffer start (for GPU binding)

            // @return True if the buffer is a valid allocated GPUBuffer.
            inline bool IsValid() const { return data != nullptr && buffer != nullptr; }
        };

        // Allocates temporary memory that the CPU can write and GPU can read. 
        // Allocation is only alive for one frame and automatically invalidated after that.
        [[nodiscard]] ScratchBuffer AllocateGPU(uint64_t dataSize, CommandList cmd)
        {
            ScratchBuffer allocation{};
            if (dataSize == 0)
                return allocation;

            GPULinearAllocator& allocator = GetFrameAllocator(cmd);

            // query the size of the current buffer (if it exists)
            const uint64_t currentBufferSize = allocator.buffer ? allocator.buffer->GetDesc().size : 0;
            const uint64_t freeSpace = currentBufferSize - allocator.offset;

            if (dataSize > freeSpace)
            {
                BufferDesc desc{};
                desc.cpuAccess = CpuAccessMode::Write;
                desc.usage = BufferUsage::ConstantBufferBit | BufferUsage::VertexBufferBit | BufferUsage::IndexBufferBit;
                allocator.alignment = GetMinOffsetAlignment(&desc);
                desc.size = AlignPow2((currentBufferSize + dataSize) * 2, allocator.alignment);
				desc.debugName = "ScratchBufferPool";

                allocator.buffer = CreateBuffer(&desc, nullptr);
                allocator.offset = 0;

                CYB_TRACE("Increasing GPU frame allocation for cmd(0x{:x}) bufferIndex {} to {:.1f}kb", (ptrdiff_t)cmd.internal_state, GetBufferIndex(), desc.size / 1024.0f);
            }

            allocation.buffer = allocator.buffer;
            allocation.offset = allocator.offset;
            allocation.data = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(allocator.buffer->MappedMemory()) + allocator.offset);
			// align the offset for the next allocation
            allocator.offset += AlignPow2(dataSize, allocator.alignment);

            assert(allocation.IsValid());
            return allocation;
        }

        // Update a gpu buffer data
        // Since it uses a GPU Copy operation, appropriate synchronization is expected
        // And it cannot be used inside a RenderPass
        void UpdateBuffer(IBuffer* buffer, const void* data, CommandList cmd, uint64_t size = ~0, uint64_t offset = 0)
        {
			assert(buffer->GetDesc().cpuAccess != CpuAccessMode::Write);
            if (buffer == nullptr || data == nullptr)
                return;

            size = std::min(buffer->GetDesc().size, size);
            if (size == 0)
                return;

            ScratchBuffer allocation = AllocateGPU(size, cmd);
            std::memcpy(allocation.data, data, size);

            CopyBuffer(buffer, offset, allocation.buffer, allocation.offset, size, cmd);
        }

        // Bind a constant buffer with data for a specific command list
        // This will be done on the CPU to an UPLOAD buffer, so this can be used inside a RenderPass
        // But this will be only visible on the command list it was bound to
        template<typename T>
        void BindDynamicConstantBuffer(const T& data, uint32_t slot, CommandList cmd)
        {
            ScratchBuffer allocation = AllocateGPU(sizeof(T), cmd);
            std::memcpy(allocation.data, &data, sizeof(T));
            BindConstantBuffer(allocation.buffer, slot, cmd, allocation.offset);
        }
    };

    inline const FormatInfo& GetFormatInfo(Format format)
    {
        static constexpr std::array formatInfoTable = std::to_array<FormatInfo>({
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
        });

        static_assert(formatInfoTable.size() == Numerical(Format::COUNT));

        assert(uint32_t(format) < uint32_t(Format::COUNT));
        if (uint32_t(format) >= uint32_t(Format::COUNT))
            return formatInfoTable[0]; // UNKNOWN

        const FormatInfo& info = formatInfoTable[(uint32_t)format];
        assert(info.format == format);
        return info;
    }

    inline GraphicsDevice*& GetDevice()
    {
        static GraphicsDevice* device = nullptr;
        return device;
    }
}

