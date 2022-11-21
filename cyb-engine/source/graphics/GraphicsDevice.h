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

#include "Core/Platform.h"
#include "Core/Mathlib.h"
#include "Core/EnumFlags.h"

#include <vector>

namespace cyb::graphics
{
    struct Texture;
    struct Shader;

    enum class BindFlag
    {
        NONE = 0,
        VERTEX_BUFFER = (1 << 0),
        INDEX_BUFFER = (1 << 1),
        CONSTANT_BUFFER = (1 << 2),
        RENDER_TARGET = (1 << 3),
        DEPTH_STENCIL = (1 << 4),
        SHADER_RESOURCE = (1 << 5)
    };
    CYB_ENABLE_BITMASK_OPERATORS(BindFlag);

    enum class ResourceMiscFlag
    {
        NONE = 0,
        BUFFER_RAW = (1 << 0),
        BUFFER_STRUCTURED = (1 << 1),
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceMiscFlag);
    
    enum class MemoryAccess
    {
        DEFAULT,                    // CPU no access, GPU read/write
        UPLOAD,                     // CPU write, GPU read
        READBACK                    // CPU read, GPU write
    };

    enum class TextureFilter
    {
        POINT,
        BILINEAR,
        TRILINEAR,
        ANISOTROPIC_LINEAR
    };

    enum class TextureAddressMode
    {
        CLAMP,
        WRAP,
        MIRROR,
        BORDER
    };

    enum class Format
    {
        UNKNOWN,
        R32G32B32A32_FLOAT,         // four-component, 128-bit floating-point format with 32-bit channels
        R8G8B8A8_UINT,              // four-component, 32-bit unsigned-integer format with 8-bit channels
        R8G8B8A8_UNORM,             // four-component, 32-bit unsigned-normalized integer format with 8-bit channels
        R16G16_FLOAT,               // two-component, 32-bit floating-point format with 16-bit channels 
        R32G32_FLOAT,               // two-component, 64-bit floating-point format with 32-bit channels
        R8_UNORM,                   // single-component, 8-bit unsigned-normalized integer swizzeled to { r, r, r, 1 }
        R32_FLOAT,                  // single-component, 32-bit floating-point format swizzeled to { r, r, r, 1 }
        R16_FLOAT,                  // single-component, 16-bit floating-point format swizzeled to { r, r, r, 1 }
        D32_FLOAT,                  // single-component, 32-bit floating-point format for depth
        D32_FLOAT_S8_UINT,	        // depth (32-bit) + stencil (8-bit)
        B8G8R8A8_UNORM,
        R32G32B32_FLOAT
    };

    enum class IndexBufferFormat
    {
        UINT16,
        UINT32,
    };

    enum class SubresourceType
    {
        SRV,                        // Shader resource view
        RTV,                        // Render target view
        DSV                         // Depth stencil view
    };

    enum class FillMode
    {
        WIREFRAME,
        SOLID
    };

    enum class CullMode
    {
        NONE,
        FRONT,
        BACK
    };

    enum class FrontFace
    {
        CCW,                        // Counter Clockwise
        CW                          // Clockwise
    };

    enum class PrimitiveTopology
    {
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP
    };

    enum class ComparisonFunc
    {
        NEVER,
        LESS,
        EQUAL,
        LESS_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_EQUAL,
        ALWAYS,
    };

    enum class DepthWriteMask
    {
        ZERO,	// Disables depth write
        ALL,	// Enables depth write
    };

    enum class StencilOp
    {
        KEEP,
        ZERO,
        REPLACE,
        INCR_SAT,
        DECR_SAT,
        INVERT,
        INCR,
        DECR,
    };

    enum class ShaderStage
    {
        VS,                         // Vertex shader
        FS,                         // Fragment shader
        GS,                         // Geometry shader
        COUNT
    };

    enum class ShaderFormat
    {
        NONE,
        SPIRV,
        GLSL
    };

    enum class QueueType
    {
        GRAPHICS,
        COMPUTE,
        Count,
    };

    enum class ResourceState
    {
        // Common resource states:
        UNDEFINED = 0,						// invalid state (don't preserve contents)
        SHADER_RESOURCE = 1 << 0,			// shader resource, read only
        SHADER_RESOURCE_COMPUTE = 1 << 1,	// shader resource, read only, non-pixel shader
        UNORDERED_ACCESS = 1 << 2,			// shader resource, write enabled
        COPY_SRC = 1 << 3,					// copy from
        COPY_DST = 1 << 4,					// copy to

        // Texture specific resource states:
        RENDERTARGET = 1 << 5,				// render target, write enabled
        DEPTHSTENCIL = 1 << 6,				// depth stencil, write enabled
        DEPTHSTENCIL_READONLY = 1 << 7,		// depth stencil, read only

        // GPUBuffer specific resource states:
        VERTEX_BUFFER = 1 << 9,				// vertex buffer, read only
        INDEX_BUFFER = 1 << 10,				// index buffer, read only
        CONSTANT_BUFFER = 1 << 11,			// constant buffer, read only
        INDIRECT_ARGUMENT = 1 << 12,		// argument buffer to DrawIndirect() or DispatchIndirect()
        RAYTRACING_ACCELERATION_STRUCTURE = 1 << 13, // acceleration structure storage or scratch
        PREDICATION = 1 << 14				// storage for predication comparison value
    };
    CYB_ENABLE_BITMASK_OPERATORS(ResourceState);

    struct GPUBufferDesc
    {
        uint64_t size = 0;
        MemoryAccess usage = MemoryAccess::DEFAULT;
        BindFlag bind_flags = BindFlag::NONE;
        ResourceMiscFlag misc_flags = ResourceMiscFlag::NONE;
        uint32_t stride = 0;        // Needed for struct buffer types
    };

    struct Viewport
    {
        float x = 0;                // Top-Left
        float y = 0;                // Top-Left
        float width = 0;
        float height = 0;
        float min_depth = 0;
        float max_depth = 1;
    };

    struct VertexInputLayout
    {
        static const uint32_t APPEND_ALIGNED_ELEMENT = ~0u; // automatically figure out AlignedByteOffset depending on Format

        struct Element
        {
            std::string inputName;
            uint32_t inputSlot = 0;
            Format format = Format::UNKNOWN;
            uint32_t aligned_byte_offset = APPEND_ALIGNED_ELEMENT;
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
        TextureFilter filter = TextureFilter::POINT;
        TextureAddressMode address_u = TextureAddressMode::WRAP;
        TextureAddressMode address_v = TextureAddressMode::WRAP;
        TextureAddressMode address_w = TextureAddressMode::WRAP;
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
            TEXTURE_1D,         // UNTESTED
            TEXTURE_2D,
            TEXTURE_3D          // NOT IMPLEMENTED
        };

        Type type = Type::TEXTURE_2D;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t array_size = 1;
        Format format = Format::UNKNOWN;
        uint32_t mip_levels = 1;
        BindFlag bind_flags = BindFlag::NONE;
        ResourceState layout = ResourceState::SHADER_RESOURCE;
    };

    struct RasterizerState
    {
        FillMode fill_mode = FillMode::SOLID;
        CullMode cull_mode = CullMode::NONE;
        FrontFace front_face = FrontFace::CCW;
        float line_width = 1.0f;
    };

    struct DepthStencilState
    {
        struct DepthStencilOp
        {
            StencilOp stencil_fail_op = StencilOp::KEEP;
            StencilOp stencil_depth_fail_op = StencilOp::KEEP;
            StencilOp stencil_pass_op = StencilOp::KEEP;
            ComparisonFunc stencil_func = ComparisonFunc::NEVER;
        };

        bool depth_enable = false;
        DepthWriteMask depth_write_mask = DepthWriteMask::ZERO;
        ComparisonFunc depth_func = ComparisonFunc::NEVER;
        bool stencil_enable = false;
        uint8_t stencil_read_mask = 0xff;
        uint8_t stencil_write_mask = 0xff;
        DepthStencilOp front_face;
        DepthStencilOp back_face;
        bool depth_bounds_test_enable = false;
    };

    struct RenderPassAttachment
    {
        enum class Type
        {
            RENDERTARGET,
            DEPTH_STENCIL
        };

        enum class LoadOp
        {
            LOAD,
            CLEAR,
            DONTCARE
        };
        
        enum class StoreOp
        {
            STORE,
            DONTCARE
        };

        Type type = Type::RENDERTARGET;
        LoadOp loadOp = LoadOp::LOAD;
        StoreOp storeOp = StoreOp::STORE;
        ResourceState initialLayout = ResourceState::UNDEFINED;	    // layout before the render pass
        ResourceState subpassLayout = ResourceState::UNDEFINED;	    // layout within the render pass
        ResourceState finalLayout = ResourceState::UNDEFINED;		// layout after the render pass
        const Texture* texture = nullptr;

        static RenderPassAttachment RenderTarget(
            const Texture* resource = nullptr,
            LoadOp load_op = LoadOp::LOAD,
            StoreOp store_op = StoreOp::STORE,
            ResourceState initial_layout = ResourceState::SHADER_RESOURCE,
            ResourceState subpass_layout = ResourceState::RENDERTARGET,
            ResourceState final_layout = ResourceState::SHADER_RESOURCE)
        {
            RenderPassAttachment attachment;
            attachment.type = Type::RENDERTARGET;
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
            LoadOp load_op = LoadOp::LOAD,
            StoreOp store_op = StoreOp::STORE,
            ResourceState initial_layout = ResourceState::DEPTHSTENCIL,
            ResourceState subpass_layout = ResourceState::DEPTHSTENCIL,
            ResourceState final_layout = ResourceState::DEPTHSTENCIL)
        {
            RenderPassAttachment attachment;
            attachment.type = Type::DEPTH_STENCIL;
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
        Format format = Format::B8G8R8A8_UNORM;
        bool fullscreen = false;
        bool vsync = true;
        float clear_color[4] = { .4f, .4f, .4f, 1 };
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
        PrimitiveTopology pt = PrimitiveTopology::TRIANGLE_LIST;
    };

    struct SubresourceData
    {
        const void* mem = nullptr;	    // pointer to the beginning of the subresource data (pointer to beginning of resource + subresource offset)
        uint32_t row_pitch = 0;			// bytes between two rows of a texture (2D and 3D textures)
        uint32_t slice_pitch = 0;		// bytes between two depth slices of a texture (3D textures only)
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
            BUFFER,
            TEXTURE,
            UNKNOWN
        };

        Type type = Type::UNKNOWN;
        void* mapped_data = nullptr;
        uint32_t mapped_rowpitch = 0;

        constexpr bool IsTexture() const { return type == Type::TEXTURE; }
        constexpr bool IsBuffer() const { return type == Type::BUFFER; }
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

    const uint32_t MAX_TEXTURE_UNITS = 4;

    struct FrameStats
    {
        uint32_t draw_calls = 0;
        uint64_t triangle_count = 0;
    };

    struct DeviceContextState
    {
        const Texture* currentTexture[MAX_TEXTURE_UNITS] = { 0 };
        const Sampler* currentSamplerState[MAX_TEXTURE_UNITS] = { 0 };
        const PipelineState* currentPipelineState = nullptr;
    };

    enum class GraphicsDeviceAPI
    {
        OpenGL,
        Vulkan
    };

    struct CommandList
    {
        void* internal_state = nullptr;
        constexpr bool IsValid() const { return internal_state != nullptr; }
    };

    static constexpr uint32_t DESCRIPTORBINDER_CBV_COUNT = 14;
    static constexpr uint32_t DESCRIPTORBINDER_SRV_COUNT = 16;
    static constexpr uint32_t DESCRIPTORBINDER_SAMPLER_COUNT = 16;
    struct DescriptorBindingTable
    {
        GPUBuffer CBV[DESCRIPTORBINDER_CBV_COUNT];
        uint64_t CBV_offset[DESCRIPTORBINDER_CBV_COUNT] = {};
        GPUResource SRV[DESCRIPTORBINDER_SRV_COUNT];
        int SRV_index[DESCRIPTORBINDER_SRV_COUNT] = {};
        Sampler SAM[DESCRIPTORBINDER_SAMPLER_COUNT];
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
        static const uint32_t BUFFERCOUNT = 2;
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
        constexpr uint32_t GetBufferIndex() const { return GetFrameCount() % BUFFERCOUNT; }

        // Returns the minimum required alignment for buffer offsets when creating subresources
        virtual uint64_t GetMinOffsetAlignment(const GPUBufferDesc* desc) const = 0;

        struct MemoryUsage
        {
            uint64_t budget = 0ull;		// Total video memory available for use by the current application (in bytes)
            uint64_t usage = 0ull;		// Used video memory by the current application (in bytes)
        };

        // Returns video memory statistics for the current application
        virtual MemoryUsage GetMemoryUsage() const = 0;

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
                desc.usage = MemoryAccess::UPLOAD;
                desc.bind_flags = BindFlag::CONSTANT_BUFFER | BindFlag::VERTEX_BUFFER | BindFlag::INDEX_BUFFER | BindFlag::SHADER_RESOURCE;
                desc.misc_flags = ResourceMiscFlag::BUFFER_RAW;
                allocator.alignment = GetMinOffsetAlignment(&desc);
                desc.size = AlignTo((allocator.buffer.desc.size + dataSize) * 2, allocator.alignment);
                CreateBuffer(&desc, nullptr, &allocator.buffer);
                SetName(&allocator.buffer, "frame_allocator");
                allocator.offset = 0;
            }

            allocation.buffer = allocator.buffer;
            allocation.offset = allocator.offset;
            allocation.data = (void*)((size_t)allocator.buffer.mapped_data + allocator.offset);

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
        case Format::R32G32B32A32_FLOAT:
            return 16u;

        case Format::R32G32B32_FLOAT:
            return 12u;

        case Format::R32G32_FLOAT:
            return 8u;

        case Format::R8G8B8A8_UINT:
        case Format::R8G8B8A8_UNORM:
        case Format::R16G16_FLOAT:
        case Format::R32_FLOAT:
        case Format::D32_FLOAT:
        case Format::B8G8R8A8_UNORM:
            return 4u;

        case Format::R16_FLOAT:
            return 2u;

        case Format::R8_UNORM:
            return 1u;

        default:
            assert(0);
            break;
        }

        return 16u;
    }
}

