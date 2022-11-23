/************************************************************************************
 GPU resource struct ordering:
    buffer
    texture
    sampler
    shader
    pipeline state
    swap chain
 ************************************************************************************/
#pragma once
#include "core/platform.h"
#include "core/Mathlib.h"
#include "core/EnumFlags.h"
#include <vector>

namespace cyb::graphics
{
    struct Texture;
    struct Shader;

    enum class BindFlags
    {
        kNone                   = 0,
        kVertexBufferBit        = (1 << 0),
        kIndexBufferBit         = (1 << 1),
        kConstantBufferBit      = (1 << 2),
        kRenderTargetBit        = (1 << 3),
        kDepthStencilBit        = (1 << 4),
        kShaderResourceBit      = (1 << 5)
    };
    CYB_ENABLE_BITMASK_OPERATORS(BindFlags);

    enum class ResourceMiscFlag
    {
        kNone                   = 0,
        kBufferRawBit           = (1 << 0),
        kBufferStructuredBit    = (1 << 1),
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceMiscFlag);
    
    enum class MemoryAccess
    {
        kDefault,                   // CPU no access, GPU read/write
        kUpload,                    // CPU write, GPU read
        kReadback                   // CPU read, GPU write
    };

    enum class TextureFilter 
    { 
        kPoint,
        kBilinear,
        kTrilinear,
        kAnisotropic
    };

    enum class TextureAddressMode 
    {
        kClamp,
        kWrap,
        kMirror,
        kBorder
    };

    enum class Format
    {
        kUnknown,
        kR32G32B32A32_Float,        // Four-component, 128-bit floating-point format with 32-bit channels
        kR8G8B8A8_Uint,             // Four-component, 32-bit unsigned-integer format with 8-bit channels
        kR8G8B8A8_Unorm,            // Four-component, 32-bit unsigned-normalized integer format with 8-bit channels
        kR16G16_Float,              // Two-component, 32-bit floating-point format with 16-bit channels 
        kR32G32_Float,              // Two-component, 64-bit floating-point format with 32-bit channels
        kR8_Unorm,                  // Single-component, 8-bit unsigned-normalized integer swizzeled to { r, r, r, 1 }
        kR32_Float,                 // Single-component, 32-bit floating-point format swizzeled to { r, r, r, 1 }
        kR16_Float,                 // Single-component, 16-bit floating-point format swizzeled to { r, r, r, 1 }
        kD32_Float,                 // Single-component, 32-bit floating-point format for depth
        kD32_Float_S8_Uint,	        // Depth (32-bit) + stencil (8-bit)
        kB8G8R8A8_Unorm,
        kR32G32B32_Float
    };

    enum class IndexBufferFormat 
    {
        kUint16,
        kUint32
    };

    enum class SubresourceType
    {
        kSRV,                       // Shader resource view
        kRTV,                       // Render target view
        kDSV                        // Depth stencil view
    };

    enum class FillMode 
    {
        kWhireframe,
        kSolid
    };

    enum class CullMode
    {
        kNone,
        kFront,
        kBack
    };

    enum class FrontFace
    {
        kCCW,
        kCW
    };

    enum class PrimitiveTopology 
    { 
        kTriangleList,
        kTriangleStrip,
        kPointList,
        kLineList,
        kLineStrip
    };

    enum class ComparisonFunc
    {
        kNever,
        kEqual,
        kNotEqual,
        kLess,
        kLessEqual,
        kGreater,
        kGreaterEqual,
        kAllways
    };

    enum class DepthWriteMask
    {
        kZero,	                    // Disables depth write
        kAll,	                    // Enables depth write
    };

    enum class StencilOp
    {
        kKeep,
        kZero,
        kReplace,
        kIncrementClamp,
        kDecrementClamp,
        kInvert,
        kIncrement,
        kDecrement,
    };

    enum class ShaderStage
    {
        kVS,                         // Vertex shader
        kFS,                         // Fragment shader
        kGS,                         // Geometry shader
        _kCount
    };

    enum class ShaderFormat
    {
        kNone,
        kSpirV,
        kGLSL
    };

    enum class QueueType
    {
        kGraphics,
        kCompute,
        _kCount
    };

    enum class ResourceState
    {
        // Common resource states:
        kUndefined = 0,                     // Invalid state (doesen't preserve contents)
        kShaderResourceBit = 1 << 0,        // Shader resource, read only
        kShaderResourceComputeBit = 1 << 1, // Shader resource, read only, non-pixel shader
        kUnorderedAccessBit = 1 << 2,       // Shader resource, write enabled
        kCopySrcBit = 1 << 3,               // Copy from
        kCopyDstBit = 1 << 4,               // Copy to

        // Texture specific resource states:
        kRenderTargetBit = 1 << 5,	        // Render target, write enabled
        kDepthStencilBit = 1 << 6,	        // Depth stencil, write enabled
        kDepthStencil_ReadOnlyBit = 1 << 7, // Depth stencil, read only

        // GPUBuffer specific resource states:
        kVertexBufferBit = 1 << 9,          // Vertex buffer, read only
        kIndexBufferBit = 1 << 10,          // Index buffer, read only
        kConstantBufferBit = 1 << 11,       // Constant buffer, read only
        kIndirectArgumentBit = 1 << 12,     // Argument buffer to DrawIndirect() or DispatchIndirect()
        kRaytracingAccelerationStructureBit = 1 << 13, // Acceleration structure storage or scratch
        kPredictionBit = 1 << 14            // Storage for predication comparison value
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceState);

    struct GPUBufferDesc
    {
        uint64_t size = 0;
        MemoryAccess usage = MemoryAccess::kDefault;
        BindFlags bindFlags = BindFlags::kNone;
        ResourceMiscFlag miscFlags = ResourceMiscFlag::kNone;
        uint32_t stride = 0;                // Needed for struct buffer types
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
        enum : uint32_t { kAppendAlignedElement = ~0u }; // automatically figure out AlignedByteOffset depending on Format

        struct Element
        {
            std::string inputName;
            uint32_t inputSlot = 0;
            Format format = Format::kUnknown;
            uint32_t alignedByteOffset = kAppendAlignedElement;
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
        TextureFilter filter = TextureFilter::kPoint;
        TextureAddressMode addressU = TextureAddressMode::kWrap;
        TextureAddressMode addressV = TextureAddressMode::kWrap;
        TextureAddressMode addressW = TextureAddressMode::kWrap;
        float lodBias = 0.0f;
        float maxAnisotropy = 16.0f;
        XMFLOAT4 borderColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        float minLOD = 0.0f;
        float maxLOD = FLT_MAX;
    };

    struct TextureDesc
    {
        enum class Type
        {
            kTexture1D,         // UNTESTED
            kTexture2D,
            kTexture3D          // NOT IMPLEMENTED
        };

        Type type = Type::kTexture2D;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arraySize = 1;
        Format format = Format::kUnknown;
        uint32_t mipLevels = 1;
        BindFlags bindFlags = BindFlags::kNone;
        ResourceState layout = ResourceState::kShaderResourceBit;
    };

    struct RasterizerState
    {
        FillMode fillMode = FillMode::kSolid;
        CullMode cullMode = CullMode::kNone;
        FrontFace frontFace = FrontFace::kCCW;
        float lineWidth = 1.0f;
    };

    struct DepthStencilState
    {
        struct DepthStencilOp
        {
            StencilOp stencilFailOp = StencilOp::kKeep;
            StencilOp stencilDepthFailOp = StencilOp::kKeep;
            StencilOp stencilPassOp = StencilOp::kKeep;
            ComparisonFunc stencilFunc = ComparisonFunc::kNever;
        };

        bool depthEnable = false;
        DepthWriteMask depthWriteMask = DepthWriteMask::kZero;
        ComparisonFunc depthFunc = ComparisonFunc::kNever;
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        DepthStencilOp frontFace;
        DepthStencilOp backFace;
        bool depthBoundsTestEnable = false;
    };

    struct RenderPassAttachment
    {
        enum class Type
        {
            kRenderTarget,
            kDepthStencil
        };

        enum class LoadOp
        {
            kLoad,
            kClear,
            kDontCare
        };
        
        enum class StoreOp
        {
            kStore,
            kDontCare
        };

        Type type = Type::kRenderTarget;
        LoadOp loadOp = LoadOp::kLoad;
        StoreOp storeOp = StoreOp::kStore;
        ResourceState initialLayout = ResourceState::kUndefined;	    // layout before the render pass
        ResourceState subpassLayout = ResourceState::kUndefined;	    // layout within the render pass
        ResourceState finalLayout = ResourceState::kUndefined;		// layout after the render pass
        const Texture* texture = nullptr;

        static RenderPassAttachment RenderTarget(
            const Texture* resource = nullptr,
            LoadOp load_op = LoadOp::kLoad,
            StoreOp store_op = StoreOp::kStore,
            ResourceState initial_layout = ResourceState::kShaderResourceBit,
            ResourceState subpass_layout = ResourceState::kRenderTargetBit,
            ResourceState final_layout = ResourceState::kShaderResourceBit)
        {
            RenderPassAttachment attachment;
            attachment.type = Type::kRenderTarget;
            attachment.texture = resource;
            attachment.loadOp = load_op;
            attachment.storeOp = store_op;
            attachment.initialLayout = initial_layout;
            attachment.subpassLayout = subpass_layout;
            attachment.finalLayout = final_layout;
            return attachment;
        }

        static RenderPassAttachment DepthStencil(
            const Texture* resource = nullptr,
            LoadOp load_op = LoadOp::kLoad,
            StoreOp store_op = StoreOp::kStore,
            ResourceState initial_layout = ResourceState::kDepthStencilBit,
            ResourceState subpass_layout = ResourceState::kDepthStencilBit,
            ResourceState final_layout = ResourceState::kDepthStencilBit)
        {
            RenderPassAttachment attachment;
            attachment.type = Type::kDepthStencil;
            attachment.texture = resource;
            attachment.loadOp = load_op;
            attachment.storeOp = store_op;
            attachment.initialLayout = initial_layout;
            attachment.subpassLayout = subpass_layout;
            attachment.finalLayout = final_layout;

            return attachment;
        }
    };

    struct RenderPassDesc
    {
        std::vector<RenderPassAttachment> attachments;
    };

    struct SwapChainDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bufferCount = 2;
        Format format = Format::kB8G8R8A8_Unorm;
        bool fullscreen = false;
        bool vsync = true;
        float clearColor[4] = { .4f, .4f, .4f, 1.0f };
    };

    struct FramebufferDesc
    {
        Texture* colorAttachment0 = nullptr;
        Texture* colorAttachment1 = nullptr;
        Texture* colorAttachment2 = nullptr;
        Texture* colorAttachment3 = nullptr;
        Texture* depthAttachment = nullptr;
    };

    struct PipelineStateDesc
    {
        const Shader* vs = nullptr;
        const Shader* gs = nullptr;
        const Shader* fs = nullptr;
        const RasterizerState* rs = nullptr;
        const DepthStencilState* dss = nullptr;
        const VertexInputLayout* il = nullptr;
        PrimitiveTopology pt = PrimitiveTopology::kTriangleList;
    };

    struct SubresourceData
    {
        const void* mem = nullptr;	    // Pointer to the beginning of the subresource data (pointer to beginning of resource + subresource offset)
        uint32_t rowPitch = 0;			// Bytes between two rows of a texture (2D and 3D textures)
        uint32_t slicePitch = 0;		// Bytes between two depth slices of a texture (3D textures only)
    };

    struct Rect
    {
        int32_t left = 0;
        int32_t top = 0;
        int32_t right = 0;
        int32_t bottom = 0;
    };

    //------------------------------------------------------------------------------
    // Render device children
    //------------------------------------------------------------------------------

    struct RenderDeviceChild
    {
        std::shared_ptr<void> internal_state;
        inline bool IsValid() const { return internal_state.get() != nullptr; }
    };

    struct GPUResource : public RenderDeviceChild
    {
        enum class Type
        {
            kUnknown,
            kBuffer,
            kTexture
        };

        Type type = Type::kUnknown;
        void* mappedData = nullptr;
        uint32_t mappedRowPitch = 0;

        constexpr bool IsTexture() const { return type == Type::kTexture; }
        constexpr bool IsBuffer() const { return type == Type::kBuffer; }
    };

    struct GPUBuffer : public GPUResource
    {
        GPUBufferDesc desc;
        const GPUBufferDesc& GetDesc() const { return desc; }
    };

    struct Texture final : public GPUResource
    {
        TextureDesc desc;
        const TextureDesc& GetDesc() const { return desc; }
    };

    struct Shader final : public RenderDeviceChild
    {
        ShaderStage stage = ShaderStage::kVS;
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

    struct RenderPass final : public RenderDeviceChild
    {
        size_t hash = 0;
        RenderPassDesc desc;
    };

    struct SwapChain final : public RenderDeviceChild
    {
        SwapChainDesc desc;

        constexpr const SwapChainDesc& GetDesc() const { return desc; }
    };

    //------------------------------------------------------------------------------
    // Render device base class / interface
    //------------------------------------------------------------------------------

    enum { kMaxTextureUnits = 4 };

    struct FrameStats
    {
        uint32_t draw_calls = 0;
        uint64_t triangle_count = 0;
    };

    struct DeviceContextState
    {
        const Texture* currentTexture[kMaxTextureUnits] = { 0 };
        const Sampler* currentSamplerState[kMaxTextureUnits] = { 0 };
        const PipelineState* currentPipelineState = nullptr;
    };

    enum class GraphicsDeviceAPI
    {
        kOpenGL,
        kVulkan
    };

    struct CommandList
    {
        void* internal_state = nullptr;
        constexpr bool IsValid() const { return internal_state != nullptr; }
    };

    enum { kDescriptorBinderCBVCount = 14 };
    enum { kDescriptorBinderSRVCount = 14 };
    enum { kDescriptorBinderSamplerCount = 14 };

    struct DescriptorBindingTable
    {
        GPUBuffer CBV[kDescriptorBinderCBVCount];
        uint64_t CBV_offset[kDescriptorBinderCBVCount] = {};
        GPUResource kSRV[kDescriptorBinderSRVCount];
        int SRV_index[kDescriptorBinderSRVCount] = {};
        Sampler SAM[kDescriptorBinderSamplerCount];
    };

    constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }
    constexpr uint64_t AlignTo(uint64_t value, uint64_t alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    class GraphicsDevice
    {
    protected:
        enum { kBufferCount = 2 };
        uint64_t frameCount = 0;
        bool validationModeEnabled = true;

    public:
        virtual ~GraphicsDevice() = default;
        virtual GraphicsDeviceAPI GetDeviceAPI() const = 0;

        virtual bool CreateSwapChain(const SwapChainDesc* desc, platform::Window* window, SwapChain* swapchain) const = 0;
        virtual bool CreateBuffer(const GPUBufferDesc* desc, const void* initData, GPUBuffer* buffer) const = 0;
        virtual bool CreateTexture(const TextureDesc* desc, const SubresourceData* init_data, Texture* texture) const = 0;
        virtual bool CreateShader(ShaderStage stage, const void* shaderBytecode, size_t bytecodeLength, Shader* shader) const = 0;
        virtual bool CreateSampler(const SamplerDesc* desc, Sampler* sampler) const = 0;
        virtual bool CreatePipelineState(const PipelineStateDesc* desc, PipelineState* pso) const = 0;
        virtual bool CreateRenderPass(const RenderPassDesc* desc, RenderPass* renderpass) const = 0;

        virtual CommandList BeginCommandList() { return CommandList{}; }
        virtual void SubmitCommandList() {}
        virtual void SetName(GPUResource* resource, const char* name) {}

        constexpr uint64_t GetFrameCount() const { return frameCount; }
        constexpr uint32_t GetBufferIndex() const { return GetFrameCount() % kBufferCount; }

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
        virtual void BeginRenderPass(const RenderPass* renderpass, CommandList cmd) = 0;
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
        //	It is only alive for one frame and automatically invalidated after that.
        GPUAllocation AllocateGPU(uint64_t dataSize, CommandList cmd)
        {
            GPUAllocation allocation;
            if (dataSize == 0)
                return allocation;

            GPULinearAllocator& allocator = GetFrameAllocator(cmd);

            const uint64_t free_space = allocator.buffer.desc.size - allocator.offset;
            if (dataSize > free_space)
            {
                GPUBufferDesc desc;
                desc.usage = MemoryAccess::kUpload;
                desc.bindFlags = BindFlags::kConstantBufferBit | BindFlags::kVertexBufferBit | BindFlags::kIndexBufferBit | BindFlags::kShaderResourceBit;
                desc.miscFlags = ResourceMiscFlag::kBufferRawBit;
                allocator.alignment = GetMinOffsetAlignment(&desc);
                desc.size = AlignTo((allocator.buffer.desc.size + dataSize) * 2, allocator.alignment);
                CreateBuffer(&desc, nullptr, &allocator.buffer);
                SetName(&allocator.buffer, "frame_allocator");
                allocator.offset = 0;
            }

            allocation.buffer = allocator.buffer;
            allocation.offset = allocator.offset;
            allocation.data = (void*)((size_t)allocator.buffer.mappedData + allocator.offset);

            allocator.offset += AlignTo(dataSize, allocator.alignment);

            assert(allocation.IsValid());
            return allocation;
        }

        // Updates a MemoryAccess::DEFAULT buffer data
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

        // Helper util to bind a constant buffer with data for a specific command list:
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

    constexpr uint32_t GetFormatStride(Format format)
    {
        switch (format)
        {
        case Format::kR32G32B32A32_Float:
            return 16u;

        case Format::kR32G32B32_Float:
            return 12u;

        case Format::kR32G32_Float:
            return 8u;

        case Format::kR8G8B8A8_Uint:
        case Format::kR8G8B8A8_Unorm:
        case Format::kR16G16_Float:
        case Format::kR32_Float:
        case Format::kD32_Float:
        case Format::kB8G8R8A8_Unorm:
            return 4u;

        case Format::kR16_Float:
            return 2u;

        case Format::kR8_Unorm:
            return 1u;

        default:
            assert(0);
            break;
        }

        return 16u;
    }
}

