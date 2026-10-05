#include <utility>
#include "core/cvar.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "systems/scene.h"
#include "systems/event_system.h"
#include "systems/profiler.h"
#include "graphics/renderer.h"
#include "graphics/image.h"
#include "graphics/shader_compiler.h"
#include "../shaders/shader_interop.h"

using namespace cyb::rhi;
using namespace cyb::scene;

namespace cyb::renderer
{
    GraphicsDevice*& device = GetDevice();

    CVar<bool> r_debugObjectAABB{ "r_debugObjectAABB", false, CVarFlag::RendererBit, "Render AABB of all objects in the scene" };
    CVar<bool> r_debugLightSources{ "r_debugLightSources", false, CVarFlag::RendererBit, "Render icon and AABB of all light sources" };
	CVar<float> r_gamme{ "r_gamma", 2.2f, 1.0f, 3.0f, CVarFlag::RendererBit, "Gamma correction value" };

    std::array<ShaderHandle, SHADERTYPE_COUNT> shaders{};
    std::array<BufferHandle, CBTYPE_COUNT> constantbuffers{};
    std::array<SamplerHandle, SSLOT_COUNT> samplerStates{};
    std::array<VertexInputLayout, VLTYPE_COUNT> input_layouts{};
    std::array<RasterizerState, RSTYPE_COUNT> rasterizers{};
    std::array<DepthStencilState, DSSTYPE_COUNT> depth_stencils{};

    std::array<PipelineStateHandle, MaterialComponent::Shadertype_Count> psoMaterial{};
    PipelineStateHandle psoOutline;
    PipelineStateHandle psoSky;

    enum DEBUGRENDERING
    {
        DEBUGRENDERING_CUBE,
        DEBUGRENDERING_COUNT
    };
    std::array<PipelineStateHandle, DEBUGRENDERING_COUNT> pso_debug{};

    enum BUILTIN_TEXTURES
    {
        BUILTIN_TEXTURE_POINTLIGHT,
        BUILTIN_TEXTURE_DIRLIGHT,
        BUILTIN_TEXTURE_COUNT
    };
    std::array<Resource, BUILTIN_TEXTURE_COUNT> builtin_textures{};

	// paths has to be '/' terminated!
    constexpr std::array SHADERPATHS = std::to_array<std::string_view>({
        "../engine/shaders/",   // on dev
        "engine/shaders/"       // on release
    });

    const IShader* GetShader(SHADERTYPE id)
    {
        assert(id < shaders.size());
        return shaders[id];
    }

    const ISampler* GetSamplerState(SSLOT id)
    {
        assert(id < samplerStates.size());
        return samplerStates[id];
    }

    const rhi::RasterizerState* GetRasterizerState(RSTYPES id)
    {
        assert(id < rasterizers.size());
        return &rasterizers[id];
    }

    const rhi::DepthStencilState* GetDepthStencilState(DSSTYPES id)
    {
        assert(id < depth_stencils.size());
        return &depth_stencils[id];
    }

    void LoadBuffers(jobsystem::JobCounter& ctx)
    {
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) {
            BufferDesc desc;
            desc.usage = BufferUsage::ConstantBufferBit;

            //
            // DEFAULT usage buffers (long lifetime, slow update, fast read)
            //
            desc.cpuAccess = CpuAccessMode::None;
            desc.size = sizeof(FrameConstants);
            desc.stride = 0;
            desc.debugName = "constantbuffers[CBTYPE_FRAME]";
            constantbuffers[CBTYPE_FRAME] = device->CreateBuffer(&desc, nullptr);

            desc.size = sizeof(CameraConstants);
            desc.stride = 0;
			desc.debugName = "constantbuffers[CBTYPE_CAMERA]";
            constantbuffers[CBTYPE_CAMERA] = device->CreateBuffer(&desc, nullptr);

            //
            // DYNAMIC usage buffers (short lifetime, fast update, slow read)
            //
            desc.cpuAccess = CpuAccessMode::None;
            desc.size = sizeof(MaterialCB);
            desc.stride = 0;
            desc.debugName = "constantbuffers[CBTYPE_MATERIAL]";
            constantbuffers[CBTYPE_MATERIAL] = device->CreateBuffer(&desc, nullptr);

            desc.size = sizeof(ImageConstants);
            desc.debugName = "constantbuffers[CBTYPE_IMAGE]";
            constantbuffers[CBTYPE_IMAGE] = device->CreateBuffer(&desc, nullptr);

            desc.size = sizeof(MiscCB);
            desc.debugName = "constantbuffers[CBTYPE_MISC]";
            constantbuffers[CBTYPE_MISC] = device->CreateBuffer(&desc, nullptr);
        });
    }

    ShaderHandle LoadShader(ShaderType stage, const std::string& filename)
    {
        std::string shaderpath{};
        for (const auto& path : SHADERPATHS)
        {
            if (std::filesystem::exists(path))
            {
                shaderpath = path;
                break;
            }
        }

        std::string fullPath = shaderpath + filename;
        std::vector<uint8_t> fileData;
        if (!filesystem::ReadFile(fullPath, fileData))
            return nullptr;

        if (!filesystem::HasExtension(filename, "spv"))
        {
            CompileShaderDesc input{};
            input.name   = fullPath;
            input.source = fileData;
            input.stage  = stage;
            input.flags  = ShaderCompilerFlags::OptimizeForSpeedBit;
            //#ifdef CYB_DEBUG_BUILD
            input.flags |= ShaderCompilerFlags::GenerateDebugInfoBit;
            //#endif

            auto output = CompileShader(input);
            if (!output.has_value())
            {
                CYB_ERROR("Failed to compile shader (filename={0}):\n{1}", filename, output.error());
                return nullptr;
            }

			ShaderDesc shaderDesc{};
			shaderDesc.stage = stage;
			shaderDesc.format = ShaderFormat::SpirV;
			shaderDesc.bytecode = output->shader.data();
			shaderDesc.bytecodeLength = output->shader.size();
			shaderDesc.debugName = filename;

            return device->CreateShader(&shaderDesc);
        }

        ShaderDesc shaderDesc{};
        shaderDesc.stage = stage;
        shaderDesc.format = ShaderFormat::GLSL;
        shaderDesc.bytecode = fileData.data();
        shaderDesc.bytecodeLength = fileData.size();
		shaderDesc.debugName = filename;

        return device->CreateShader(&shaderDesc);
    }

    static void LoadSamplerStates()
    {
        SamplerDesc desc{};
        desc.maxAnisotropy = 1.0f;
        desc.borderColor = XMFLOAT4(0, 0, 0, 0);
        desc.lodBias = 0.0f;
        desc.minLOD = 0.0f;
        desc.maxLOD = FLT_MAX;

		// point filtering states
        desc.filter = Filtering::None;
        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Wrap;
        samplerStates[SSLOT_POINT_WRAP] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Mirror;
        samplerStates[SSLOT_POINT_MIRROR] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Clamp;
        samplerStates[SSLOT_POINT_CLAMP] = device->CreateSampler(&desc);

        // bilinear filtering states
        desc.filter = Filtering::Min | Filtering::Mag;
        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Wrap;
        samplerStates[SSLOT_BILINEAR_WRAP] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Mirror;
        samplerStates[SSLOT_BILINEAR_MIRROR] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Clamp;
        samplerStates[SSLOT_BILINEAR_CLAMP] = device->CreateSampler(&desc);

        // trilinear filtering states
        desc.filter = Filtering::Min | Filtering::Mag | Filtering::Mip;
        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Wrap;
        samplerStates[SSLOT_TRILINEAR_WRAP] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Mirror;
        samplerStates[SSLOT_TRILINEAR_MIRROR] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Clamp;
        samplerStates[SSLOT_TRILINEAR_CLAMP] = device->CreateSampler(&desc);

        // anisotropic filtering states
        desc.filter = Filtering::Min | Filtering::Mag | Filtering::Mip;
        desc.maxAnisotropy = 16.0f;

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Wrap;
        samplerStates[SSLOT_ANISO_WRAP] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Mirror;
        samplerStates[SSLOT_ANISO_MIRROR] = device->CreateSampler(&desc);

        desc.addressU = desc.addressV = desc.addressW = SamplerAddressMode::Clamp;
        samplerStates[SSLOT_ANISO_CLAMP] = device->CreateSampler(&desc);
    }

    static void LoadBuiltinTextures(jobsystem::JobCounter& ctx)
    {
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { builtin_textures[BUILTIN_TEXTURE_POINTLIGHT] = resourcemanager::LoadFile("textures/light_point.png"); });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { builtin_textures[BUILTIN_TEXTURE_DIRLIGHT] = resourcemanager::LoadFile("textures/light_directional.png"); });
    }

    static void LoadShaders()
    {
        jobsystem::JobCounter jobCount{};

        // vertex shaders
        jobsystem::Execute(jobCount, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_FLAT_SHADING] =
            {
                { "in_position", 0, scene::MeshComponent::Vertex_Pos::FORMAT },
                { "in_color",    1, scene::MeshComponent::Vertex_Col::FORMAT }
            };
            shaders[VSTYPE_FLAT_SHADING] = LoadShader(ShaderType::Vertex, "flat_shader.vert");
        });
        jobsystem::Execute(jobCount, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_SKY] =
            {
                { "in_pos",   0, scene::MeshComponent::Vertex_Pos::FORMAT }
            };
            shaders[VSTYPE_SKY] = LoadShader(ShaderType::Vertex, "sky.vert");
        });
        jobsystem::Execute(jobCount, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_DEBUG_LINE] =
            {
                { "in_position", 0, Format::RGBA32_FLOAT },
                { "in_color",    0, Format::RGBA32_FLOAT }
            };
            shaders[VSTYPE_DEBUG_LINE] = LoadShader(ShaderType::Vertex, "debug_line.vert");
        });

        jobsystem::Execute(jobCount, [] (jobsystem::JobArgs) { shaders[VSTYPE_POSTPROCESS] = LoadShader(ShaderType::Vertex, "postprocess.vert"); });

        // geometry shaders
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[GSTYPE_FLAT_SHADING] = LoadShader(ShaderType::Geometry, "flat_shader.geom"); });
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[GSTYPE_FLAT_DISNEY_SHADING] = LoadShader(ShaderType::Geometry, "flat_shader_disney.geom"); });
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[GSTYPE_FLAT_UNLIT] = LoadShader(ShaderType::Geometry, "flat_shader_unlit.geom"); });

		// pixel shaders
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[FSTYPE_FLAT_SHADING] = LoadShader(ShaderType::Pixel, "flat_shader.frag"); });
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[FSTYPE_SKY] = LoadShader(ShaderType::Pixel, "sky.frag"); });
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[FSTYPE_POSTPROCESS_OUTLINE] = LoadShader(ShaderType::Pixel, "outline.frag"); });
        jobsystem::Execute(jobCount, [](jobsystem::JobArgs) { shaders[FSTYPE_DEBUG_LINE] = LoadShader(ShaderType::Pixel, "debug_line.frag"); });

        jobsystem::Wait(jobCount);

        {
            DepthStencilState dsd;
            dsd.depthEnable = true;
            dsd.depthWriteMask = DepthWriteMask::All;
            dsd.depthFunc = ComparisonFunc::Greater;

            dsd.stencilEnable = true;
            dsd.stencilReadMask = 0;
            dsd.stencilWriteMask = 0xFF;
            dsd.frontFace.stencilFunc = ComparisonFunc::Allways;
            dsd.frontFace.stencilPassOp = StencilOp::Replace;
            dsd.frontFace.stencilFailOp = StencilOp::Keep;
            dsd.frontFace.stencilDepthFailOp = StencilOp::Keep;
            dsd.backFace.stencilFunc = ComparisonFunc::Allways;
            dsd.backFace.stencilPassOp = StencilOp::Replace;
            dsd.backFace.stencilFailOp = StencilOp::Keep;
            dsd.backFace.stencilDepthFailOp = StencilOp::Keep;
            depth_stencils[DSSTYPE_DEFAULT] = dsd;

            dsd.depthEnable = false;
            dsd.stencilEnable = false;
            depth_stencils[DSSTYPE_DEPTH_DISABLED] = dsd;

            dsd.depthEnable = true;
            dsd.stencilEnable = false;
            dsd.depthWriteMask = DepthWriteMask::Zero;
            dsd.depthFunc = ComparisonFunc::GreaterOrEqual;
            depth_stencils[DSSTYPE_DEPTH_READ] = dsd;

            dsd.depthEnable = true;
            dsd.stencilEnable = true;
            dsd.depthWriteMask = DepthWriteMask::All;
            dsd.depthFunc = ComparisonFunc::GreaterOrEqual;
            depth_stencils[DSSTYPE_SKY] = dsd;
        }
        {
            RasterizerState rs;
            rs.polygonMode = PolygonMode::Fill;
            rs.cullMode = CullMode::Back;
            rs.frontFace = FrontFace::CCW;
            rasterizers[RSTYPE_FRONT] = rs;

            rs.polygonMode = PolygonMode::Fill;
            rs.cullMode = CullMode::Front;
            rs.frontFace = FrontFace::CCW;
            rasterizers[RSTYPE_BACK] = rs;

            rs.polygonMode = PolygonMode::Fill;
            rs.cullMode = CullMode::None;
            rs.frontFace = FrontFace::CCW;
            rasterizers[RSTYPE_DOUBLESIDED] = rs;

            rs.polygonMode = PolygonMode::Line;
            rs.cullMode = CullMode::Back;
            rs.frontFace = FrontFace::CCW;
            rasterizers[RSTYPE_WIRE] = rs;

            rs.polygonMode = PolygonMode::Line;
            rs.cullMode = CullMode::None;
            rs.frontFace = FrontFace::CCW;
            rasterizers[RSTYPE_WIRE_DOUBLESIDED] = rs;

            rs.polygonMode = PolygonMode::Fill;
            rs.cullMode = CullMode::None;
            rs.frontFace = FrontFace::CW;
            rasterizers[RSTYPE_SKY] = rs;
        }

        {
            // MaterialComponent::Shadertype_BDRF
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_SHADING);
            desc.ps = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            psoMaterial[MaterialComponent::Shadertype_BDRF] = device->CreatePipelineState(&desc);
        }
        {
            // MaterialComponent::Shadertype_Disney_BDRF
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_DISNEY_SHADING);
            desc.ps = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            psoMaterial[MaterialComponent::Shadertype_Disney_BDRF] = device->CreatePipelineState(&desc);
        }
        {
            // MaterialComponent::Shadertype_Unlit
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_UNLIT);
            desc.ps = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            psoMaterial[MaterialComponent::Shadertype_Unlit] = device->CreatePipelineState(&desc);
        }
        {
            // PSO_OUTLINE
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_POSTPROCESS);
            desc.ps = GetShader(FSTYPE_POSTPROCESS_OUTLINE);
            desc.rs = &rasterizers[RSTYPE_DOUBLESIDED];
            desc.dss = &depth_stencils[DSSTYPE_DEPTH_DISABLED];
            desc.pt = PrimitiveTopology::TriangleStrip;
            psoOutline = device->CreatePipelineState(&desc);
        }
        {
            // PSO_SKY
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_SKY);
            desc.ps = GetShader(FSTYPE_SKY);
            desc.rs = &rasterizers[RSTYPE_SKY];
            desc.dss = &depth_stencils[DSSTYPE_SKY];
            desc.pt = PrimitiveTopology::TriangleStrip;
            psoSky = device->CreatePipelineState(&desc);
        }
        {
            // DEBUGRENDERING_CUBE
            PipelineStateDesc desc{};
            desc.vs = GetShader(VSTYPE_DEBUG_LINE);
            desc.ps = GetShader(FSTYPE_DEBUG_LINE);
            desc.rs = &rasterizers[RSTYPE_WIRE_DOUBLESIDED];
            desc.dss = &depth_stencils[DSSTYPE_DEPTH_READ];
            desc.il = &input_layouts[VLTYPE_DEBUG_LINE];
            desc.pt = PrimitiveTopology::LineList;
            pso_debug[DEBUGRENDERING_CUBE] = device->CreatePipelineState(&desc);
        }

        jobsystem::Wait(jobCount);
    }

    void ReloadShaders()
    {
        device->ClearPipelineStateCache();
        eventsystem::FireEvent(eventsystem::Event_ReloadShaders, 0);
    }

    void Initialize()
    {
        Timer timer;
        GetCamera().CreatePerspective(1.78f, 0.1f, 1000.0f, 70.0f);

        jobsystem::JobCounter ctx;
        LoadBuiltinTextures(ctx);
        LoadBuffers(ctx);
        LoadShaders();
        LoadSamplerStates();
        Image_Initialize();

        jobsystem::Wait(ctx);

        static eventsystem::Handle handle = eventsystem::Subscribe(eventsystem::Event_ReloadShaders, [] (uint64_t userdata) { LoadShaders(); });
        CYB_INFO("Renderer initialized in {:.2f}ms", timer.ElapsedMilliseconds());
    }

    void SceneView::Reset(const scene::Scene* scene, const scene::CameraComponent* camera)
    {
        assert(scene);
        assert(camera);

        objectIndexes.clear();
        lightIndexes.clear();
        objectCount = 0;
        lightCount = 0;
        this->scene = scene;
        this->camera = camera;

        {
            CYB_PROFILE_CPU_SCOPE("Frustum Culling");
            const Frustum& cameraFrustum = camera->frustum;

            // perform camera frustum culling to all objects aabb and
            // store all visible objects in the view
            objectIndexes.resize(scene->objects.Size());
            for (size_t objectIndex = 0; objectIndex < scene->objects.Size(); ++objectIndex)
            {
                const AxisAlignedBox& aabb = scene->aabb_objects[objectIndex];
                const scene::ObjectComponent& object = scene->objects[objectIndex];
                if (HasFlag(object.flags, scene::ObjectComponent::Flags::RenderableBit) &&
                    cameraFrustum.IntersectsBoundingBox(aabb))
                {
                    objectIndexes[objectCount] = static_cast<uint32_t>(objectIndex);
                    ++objectCount;
                }
            }

            // perform basic camera frustum calling to all light sources
            // all directional lights will be added
            lightIndexes.resize(scene->lights.Size());
            for (size_t lightIndex = 0; lightIndex < scene->lights.Size(); ++lightIndex)
            {
                const scene::LightComponent& light = scene->lights[lightIndex];
                if (!light.IsAffectingScene())
                    continue;

                const AxisAlignedBox& aabb = scene->aabb_lights[lightIndex];
                if (cameraFrustum.IntersectsBoundingBox(aabb) ||
                    light.GetType() == LightType::Directional)
                {
                    lightIndexes[lightCount] = lightIndex;
                    ++lightCount;
                }
            }

            objectIndexes.resize(objectCount);
            lightIndexes.resize(lightCount);
        }
    }

    void UpdatePerFrameData(const SceneView& view, float time, FrameConstants& frameCB)
    {
        frameCB.time = time;
        frameCB.gamma = r_gamme.GetValue();

        // setup weather
        const scene::WeatherComponent& weather = view.scene->weather;
        frameCB.horizon = weather.horizon;
        frameCB.zenith = weather.zenith;
        frameCB.drawSun = weather.drawSun;
        frameCB.mostImportantLightIndex = 0;
        frameCB.fog = XMFLOAT4(weather.fogStart, weather.fogEnd, weather.fogHeight, 1.0f / (weather.fogEnd - weather.fogStart));
        frameCB.cloudiness = weather.cloudiness;
        frameCB.cloudTurbulence = weather.cloudTurbulence;
        frameCB.cloudHeight = weather.cloudHeight;
        frameCB.windSpeed = weather.windSpeed;

        // setup lightsources
        float brightestLight = 0.0f;
        frameCB.numLights = std::min((uint32_t)SHADER_MAX_LIGHTSOURCES, view.lightCount);
        for (uint32_t i = 0; i < frameCB.numLights; ++i)
        {
            const uint32_t lightIndex = view.lightIndexes[i];
            const scene::LightComponent& light = view.scene->lights[lightIndex];

            if (light.energy > brightestLight)
            {
                brightestLight = light.energy;
                frameCB.mostImportantLightIndex = i;
            }

            LightSource& lightConstants = frameCB.lights[i];
            lightConstants.type = static_cast<uint32_t>(light.GetType());
            lightConstants.position = XMFLOAT4(light.position.x, light.position.y, light.position.z, 0.0f);
            lightConstants.direction = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            lightConstants.color = XMFLOAT4(light.color.x, light.color.y, light.color.z, 0.0f);
            lightConstants.energy = light.energy;
            lightConstants.range = light.range;
        }

        // sort the lights by type, first directional, then point
        auto first = frameCB.lights;
        auto last = frameCB.lights + frameCB.numLights;
        auto pointLightBegin = std::partition(first, last, [] (const auto& light) {
            return light.type == LIGHTSOURCE_TYPE_DIRECTIONAL;
        });

        frameCB.pointLightsOffset = static_cast<uint32_t>(pointLightBegin - first);
    }

    void UpdateRenderData(const SceneView& view, const FrameConstants& frameCB, rhi::CommandList cmd)
    {
        device->BeginEvent("UpdateRenderData", cmd);
        device->UpdateBuffer(constantbuffers[CBTYPE_FRAME], &frameCB, cmd);
        device->EndEvent(cmd);
    }

    void BindCameraCB(const scene::CameraComponent* camera, rhi::CommandList cmd)
    {
        assert(camera);

        CameraConstants cc{};
        XMStoreFloat4x4(&cc.proj, camera->projection);
        XMStoreFloat4x4(&cc.view, camera->view);
        XMStoreFloat4x4(&cc.vp, camera->VP);
        XMStoreFloat4x4(&cc.inv_proj, camera->invProjection);
        XMStoreFloat4x4(&cc.inv_view, camera->invView);
        XMStoreFloat4x4(&cc.inv_vp, camera->invVP);
        cc.pos = XMFLOAT4(camera->pos.x, camera->pos.y, camera->pos.z, 1.0f);

        device->UpdateBuffer(constantbuffers[CBTYPE_CAMERA], &cc, cmd);
    }

    void DrawScene(const SceneView& view, CommandList cmd)
    {
        device->BeginEvent("DrawScene", cmd);

        device->BindConstantBuffer(constantbuffers[CBTYPE_FRAME], CBSLOT_FRAME, cmd);
        device->BindConstantBuffer(constantbuffers[CBTYPE_CAMERA], CBSLOT_CAMERA, cmd);

        uint8_t prevUserStencilRef = 0;
        device->BindStencilRef(0, cmd);

        // Draw all visible objects
        for (uint32_t objectIndex : view.objectIndexes)
        {
            const ObjectComponent& object = view.scene->objects[objectIndex];

            if (object.userStencilRef != prevUserStencilRef)
            {
                prevUserStencilRef = object.userStencilRef;
                device->BindStencilRef(object.userStencilRef, cmd);
            }

            const MeshComponent& mesh = view.scene->meshes[object.meshIndex];
            if (mesh.vertex_buffer_col)
            {
                std::array<const rhi::IBuffer*, 2> vertex_buffers = {
                    mesh.vertex_buffer_pos,
                    mesh.vertex_buffer_col
                };

                std::array<uint32_t, 2> strides = {
                    sizeof(scene::MeshComponent::Vertex_Pos),
                    sizeof(scene::MeshComponent::Vertex_Col)
                };

                device->BindVertexBuffers(vertex_buffers.data(), vertex_buffers.size(), strides.data(), nullptr, cmd);
                device->BindIndexBuffer(mesh.index_buffer, IndexBufferFormat::Uint32, 0, cmd);
            }
            else
            {
                //device->BindVertexBuffer(&mesh->vertex_buffer_pos);
            }

            const TransformComponent& transform = view.scene->transforms[object.transformIndex];
            MiscCB cb{};
            XMMATRIX W = transform.world;
            XMStoreFloat4x4(&cb.g_xModelMatrix, XMMatrixTranspose(W));
            XMStoreFloat4x4(&cb.g_xTransform, XMMatrixTranspose(W * view.camera->VP));
            device->BindDynamicConstantBuffer(cb, CBSLOT_MISC, cmd);

            for (const auto& subset : mesh.subsets)
            {
                // Setup Object constant buffer
                const MaterialComponent& material = view.scene->materials[subset.materialIndex];
                MaterialCB material_cb;
                material_cb.baseColor = material.baseColor;
                material_cb.roughness = material.roughness;
                material_cb.metalness = material.metalness;
                device->BindDynamicConstantBuffer(material_cb, CBSLOT_MATERIAL, cmd);

                const PipelineStateHandle pso = psoMaterial[material.shaderType];
                device->BindPipelineState(pso, cmd);
                device->DrawIndexed(subset.indexCount, subset.indexOffset, 0, cmd);
            }
        }

        device->EndEvent(cmd);
    }

    void DrawSky(const scene::CameraComponent* camera, CommandList cmd)
    {
        device->BeginEvent("DrawSky", cmd);
        device->BindStencilRef(255, cmd);
        device->BindPipelineState(psoSky, cmd);

        device->BindConstantBuffer(constantbuffers[CBTYPE_FRAME], CBSLOT_FRAME, cmd);
        device->BindConstantBuffer(constantbuffers[CBTYPE_CAMERA], CBSLOT_CAMERA, cmd);

        device->Draw(3, 0, cmd);
        device->EndEvent(cmd);
    }

    void DrawDebugScene(const SceneView& view, CommandList cmd)
    {
        static BufferHandle wirecube_vb;
        static BufferHandle wirecube_ib;

        device->BeginEvent("DrawDebugScene", cmd);

        if (!wirecube_vb)
        {
            const XMFLOAT4 min = XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f);
            const XMFLOAT4 max = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            const XMFLOAT4 verts[] = {
                min,                                 XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(min.x, max.y, min.z, 1.0f), XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(min.x, max.y, max.z, 1.0f), XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(min.x, min.y, max.z, 1.0f), XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(max.x, min.y, min.z, 1.0f), XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(max.x, max.y, min.z, 1.0f), XMFLOAT4(1, 1, 1, 1),
                max,                                 XMFLOAT4(1, 1, 1, 1),
                XMFLOAT4(max.x, min.y, max.z, 1.0f), XMFLOAT4(1, 1, 1, 1)
            };

            BufferDesc vertexbuffer_desc{};
            vertexbuffer_desc.cpuAccess = CpuAccessMode::None;
            vertexbuffer_desc.size = sizeof(verts);
            vertexbuffer_desc.usage = BufferUsage::VertexBufferBit;
            wirecube_vb = device->CreateBuffer(&vertexbuffer_desc, &verts);

            const uint16_t indices[] = {
                0,1,1,2,0,3,0,4,1,5,4,5,
                5,6,4,7,2,6,3,7,2,3,6,7
            };

            BufferDesc indexbuffer_desc{};
            indexbuffer_desc.cpuAccess = CpuAccessMode::None;
            indexbuffer_desc.size = sizeof(indices);
            indexbuffer_desc.usage = BufferUsage::IndexBufferBit;
            wirecube_ib = device->CreateBuffer(&indexbuffer_desc, &indices);
        }

        // Draw bounding boxes for all visible objects
        if (r_debugObjectAABB.GetValue())
        {
            device->BeginEvent("DebugObjectAABB", cmd);
            device->BindPipelineState(pso_debug[DEBUGRENDERING_CUBE], cmd);

            const std::array<const rhi::IBuffer*, 1> vbs {
                wirecube_vb,
            };
            const std::array<uint32_t, 1> strides {
                sizeof(XMFLOAT4) + sizeof(XMFLOAT4),
            };
            device->BindVertexBuffers(vbs.data(), vbs.size(), strides.data(), nullptr, cmd);
            device->BindIndexBuffer(wirecube_ib, IndexBufferFormat::Uint16, 0, cmd);

            MaterialCB material_cb;
            material_cb.baseColor = XMFLOAT4(1.0f, 0.933f, 0.6f, 1.0f);
            device->BindDynamicConstantBuffer(material_cb, CBSLOT_MATERIAL, cmd);

            for (uint32_t objectIndex : view.objectIndexes)
            {
                const AxisAlignedBox& aabb = view.scene->aabb_objects[objectIndex];
                MiscCB misc_cb;
                XMStoreFloat4x4(&misc_cb.g_xTransform, XMMatrixTranspose(aabb.GetAsBoxMatrix() * view.camera->VP));
                device->BindDynamicConstantBuffer(misc_cb, CBSLOT_MISC, cmd);

                device->DrawIndexed(24, 0, 0, cmd);
            }
            device->EndEvent(cmd);
        }

        if (r_debugLightSources.GetValue())
        {
            device->BeginEvent("DebugLightSources", cmd);

            // Draw icons over all the light sources
            for (uint32_t lightIndex : view.lightIndexes)
            {
                const ecs::Entity lightID = view.scene->lights.GetEntity(lightIndex);
                const scene::LightComponent& light = view.scene->lights[lightIndex];
                const scene::TransformComponent* transform = view.scene->transforms.GetComponent(lightID);

                float dist = Distance(transform->translation_local, view.camera->pos) * 0.05f;
                renderer::ImageParams params;
                params.EnableDepthTest();
                params.position = transform->translation_local;
                params.size = XMFLOAT2(dist, dist);

                XMMATRIX invR = view.camera->invRotation;
                XMMATRIX P = view.camera->VP;
                params.customRotation = &invR;
                params.customProjection = &P;

                device->BindSampler(GetSamplerState(renderer::SSLOT_BILINEAR_CLAMP), 0, cmd);

                switch (light.GetType())
                {
                case scene::LightType::Directional:
                    renderer::DrawImage(builtin_textures[BUILTIN_TEXTURE_DIRLIGHT].GetTexture(), params, cmd);
                    break;
                case scene::LightType::Point:
                    renderer::DrawImage(builtin_textures[BUILTIN_TEXTURE_POINTLIGHT].GetTexture(), params, cmd);
                    break;
                default:
                    break;
                }
            }

            // Draw all the aabb boxes for light sources
            device->BindPipelineState(pso_debug[DEBUGRENDERING_CUBE], cmd);
            const std::array<const rhi::IBuffer*, 1> vbs {
                wirecube_vb,
            };
            const std::array<uint32_t, 1> strides {
                sizeof(XMFLOAT4) + sizeof(XMFLOAT4),
            };
            device->BindVertexBuffers(vbs.data(), vbs.size(), strides.data(), nullptr, cmd);
            device->BindIndexBuffer(wirecube_ib, IndexBufferFormat::Uint16, 0, cmd);

            MaterialCB cbMaterial;
            cbMaterial.baseColor = XMFLOAT4(0.666f, 0.874f, 0.933f, 1.0f);
            device->BindDynamicConstantBuffer(cbMaterial, CBSLOT_MATERIAL, cmd);

            for (uint32_t lightIndex : view.lightIndexes)
            {
                const LightComponent& light = view.scene->lights[lightIndex];

                if (light.type == LightType::Point)
                {
                    const AxisAlignedBox& aabb = view.scene->aabb_lights[lightIndex];
                    MiscCB cbMisc;
                    XMStoreFloat4x4(&cbMisc.g_xTransform, XMMatrixTranspose(aabb.GetAsBoxMatrix() * view.camera->VP));
                    device->BindDynamicConstantBuffer(cbMisc, CBSLOT_MISC, cmd);
                    device->DrawIndexed(24, 0, 0, cmd);
                }
            }

            device->EndEvent(cmd);
        }

        device->EndEvent(cmd);
    }

    void Postprocess_Outline(
        const rhi::ITexture* input,
        CommandList cmd,
        float thickness,
        float threshold,
        const XMFLOAT4& color)
    {
        device->BeginEvent("Postprocess_Outline", cmd);

        device->BindPipelineState(psoOutline, cmd);
        device->BindSampler(samplerStates[SSLOT_POINT_CLAMP], 0, cmd);
        device->BindResource(input, 0, cmd);

        PostProcess postprocess = {};
        postprocess.param0.x = thickness;
        postprocess.param0.y = threshold;
        postprocess.param0.z = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
        postprocess.param1.x = color.x;
        postprocess.param1.y = color.y;
        postprocess.param1.z = color.z;
        postprocess.param1.w = color.w;
        device->PushConstants(&postprocess, sizeof(postprocess), cmd);
        device->Draw(3, 0, cmd);

        device->EndEvent(cmd);
    }
}