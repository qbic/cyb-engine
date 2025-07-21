#include "core/cvar.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "systems/scene.h"
#include "systems/event_system.h"
#include "systems/profiler.h"
#include "graphics/renderer.h"
#include "graphics/image.h"
#include "graphics/shader_compiler.h"
#include "../../shaders/shader_interop.h"

using namespace cyb::rhi;
using namespace cyb::scene;

namespace cyb::renderer
{
    GraphicsDevice*& device = GetDevice();

    CVar<bool> r_debugObjectAABB{ "r_debugObjectAABB", false, CVarFlag::RendererBit, "Render AABB of all objects in the scene" };
    CVar<bool> r_debugLightSources{ "r_debugLightSources", false, CVarFlag::RendererBit, "Render icon and AABB of all light sources" };
    
    Shader shaders[SHADERTYPE_COUNT];
    GPUBuffer constantbuffers[CBTYPE_COUNT];
    Sampler samplerStates[SSLOT_COUNT] = {};
    VertexInputLayout input_layouts[VLTYPE_COUNT] = {};
    RasterizerState rasterizers[RSTYPE_COUNT];
    DepthStencilState depth_stencils[DSSTYPE_COUNT];

    PipelineState psoMaterial[MaterialComponent::Shadertype_Count];
    PipelineState psoOutline;
    PipelineState psoSky;

    enum DEBUGRENDERING
    {
        DEBUGRENDERING_CUBE,
        DEBUGRENDERING_COUNT
    };
    PipelineState pso_debug[DEBUGRENDERING_COUNT];

    enum BUILTIN_TEXTURES
    {
        BUILTIN_TEXTURE_POINTLIGHT,
        BUILTIN_TEXTURE_DIRLIGHT,
        BUILTIN_TEXTURE_COUNT
    };
    Resource builtin_textures[BUILTIN_TEXTURE_COUNT];

    std::string SHADERPATHS[] = {
        "../engine/shaders/",   // on dev
        "engine/shaders/"       // on release
    };

    float GAMMA = 2.2f;

    const Shader* GetShader(SHADERTYPE id)
    {
        assert(id < SHADERTYPE_COUNT);
        return &shaders[id];
    }

    const Sampler* GetSamplerState(SSLOT id)
    {
        assert(id < SSLOT_COUNT);
        return &samplerStates[id];
    }

    const rhi::RasterizerState* GetRasterizerState(RSTYPES id)
    {
        assert(id < RSTYPE_COUNT);
        return &rasterizers[id];
    }

    const rhi::DepthStencilState* GetDepthStencilState(DSSTYPES id)
    {
        assert(id < DSSTYPE_COUNT);
        return &depth_stencils[id];
    }

    void LoadBuffers(jobsystem::Context& ctx)
    {
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) {
            GPUBufferDesc desc;
            desc.bindFlags = BindFlags::ConstantBufferBit;

            //
            // DEFAULT usage buffers (long lifetime, slow update, fast read)
            //
            desc.usage = MemoryAccess::Default;
            desc.size = sizeof(FrameConstants);
            desc.stride = 0;
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_FRAME]);
            device->SetName(&constantbuffers[CBTYPE_FRAME], "constantbuffers[CBTYPE_FRAME]");

            desc.size = sizeof(CameraConstants);
            desc.stride = 0;
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_CAMERA]);
            device->SetName(&constantbuffers[CBTYPE_CAMERA], "constantbuffers[CBTYPE_CAMERA]");

            //
            // DYNAMIC usage buffers (short lifetime, fast update, slow read)
            //
            desc.usage = MemoryAccess::Default;
            desc.size = sizeof(MaterialCB);
            desc.stride = 0;
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_MATERIAL]);
            device->SetName(&constantbuffers[CBTYPE_MATERIAL], "constantbuffers[CBTYPE_MATERIAL]");

            desc.size = sizeof(ImageConstants);
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_IMAGE]);
            device->SetName(&constantbuffers[CBTYPE_IMAGE], "constantbuffers[CBTYPE_IMAGE]");

            desc.size = sizeof(MiscCB);
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_MISC]);
            device->SetName(&constantbuffers[CBTYPE_MISC], "constantbuffers[CBTYPE_MISC]");
        });
    }

    bool LoadShader(ShaderStage stage, Shader& shader, const std::string& filename)
    {
        std::string shaderpath = "INVALID/";
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
            return false;

        if (!filesystem::HasExtension(filename, "spv"))
        {
            ShaderCompilerInput input = {};
            input.format = ShaderFormat::GLSL;
            input.name = fullPath;
            input.shadersource = (uint8_t*)fileData.data();
            input.shadersize = fileData.size();
            input.stage = stage;
            //#ifdef CYB_DEBUG_BUILD
            input.flags |= ShaderCompilerFlags::GenerateDebugInfoBit;
            //#endif

            ShaderCompilerOutput output;
            if (!CompileShader(&input, &output))
            {
                CYB_ERROR("Failed to compile shader (filename={0}):\n{1}", filename, output.error_message);
                return false;
            }

            bool success = device->CreateShader(stage, output.shaderdata, output.shadersize, &shader);
            if (success)
                device->SetName(&shader, filename.c_str());

            return success;
        }

        return device->CreateShader(stage, fileData.data(), fileData.size(), &shader);
    }

    static void LoadSamplerStates()
    {
        SamplerDesc desc;
        desc.maxAnisotropy = 1;
        desc.borderColor = XMFLOAT4(0, 0, 0, 0);
        desc.lodBias = 0.0f;
        desc.minLOD = 0.0f;
        desc.maxLOD = FLT_MAX;

        // Point filtering states
        desc.filter = TextureFilter::Point;
        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Wrap;
        device->CreateSampler(&desc, &samplerStates[SSLOT_POINT_WRAP]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Mirror;
        device->CreateSampler(&desc, &samplerStates[SSLOT_POINT_MIRROR]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Clamp;
        device->CreateSampler(&desc, &samplerStates[SSLOT_POINT_CLAMP]);

        // BiLinear filtering states
        desc.filter = TextureFilter::Bilinear;
        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Wrap;
        device->CreateSampler(&desc, &samplerStates[SSLOT_BILINEAR_WRAP]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Mirror;
        device->CreateSampler(&desc, &samplerStates[SSLOT_BILINEAR_MIRROR]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Clamp;
        device->CreateSampler(&desc, &samplerStates[SSLOT_BILINEAR_CLAMP]);

        // TriLinearfiltering states
        desc.filter = TextureFilter::Trilinear;
        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Wrap;
        device->CreateSampler(&desc, &samplerStates[SSLOT_TRILINEAR_WRAP]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Mirror;
        device->CreateSampler(&desc, &samplerStates[SSLOT_TRILINEAR_MIRROR]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Clamp;
        device->CreateSampler(&desc, &samplerStates[SSLOT_TRILINEAR_CLAMP]);

        // Anisotropic filtering states
        desc.filter = TextureFilter::Anisotropic;
        desc.maxAnisotropy = 16;
        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Wrap;
        device->CreateSampler(&desc, &samplerStates[SSLOT_ANISO_WRAP]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Mirror;
        device->CreateSampler(&desc, &samplerStates[SSLOT_ANISO_MIRROR]);

        desc.addressU = desc.addressV = desc.addressW = TextureAddressMode::Clamp;
        device->CreateSampler(&desc, &samplerStates[SSLOT_ANISO_CLAMP]);
    }

    static void LoadBuiltinTextures(jobsystem::Context& ctx)
    {
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { builtin_textures[BUILTIN_TEXTURE_POINTLIGHT] = resourcemanager::LoadFile("textures/light_point.png"); });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { builtin_textures[BUILTIN_TEXTURE_DIRLIGHT] = resourcemanager::LoadFile("textures/light_directional.png"); });
    }

    static void LoadShaders()
    {
        jobsystem::Context ctx = {};

        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_FLAT_SHADING] =
            {
                { "in_position", 0, scene::MeshComponent::Vertex_Pos::FORMAT },
                { "in_color",    1, scene::MeshComponent::Vertex_Col::FORMAT }
            };
            LoadShader(ShaderStage::VS, shaders[VSTYPE_FLAT_SHADING], "flat_shader.vert");
        });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_SKY] =
            {
                { "in_pos",   0, scene::MeshComponent::Vertex_Pos::FORMAT }
            };
            LoadShader(ShaderStage::VS, shaders[VSTYPE_SKY], "sky.vert");
        });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) {
            input_layouts[VLTYPE_DEBUG_LINE] =
            {
                { "in_position", 0, Format::R32G32B32A32_Float },
                { "in_color",    0, Format::R32G32B32A32_Float }
            };
            LoadShader(ShaderStage::VS, shaders[VSTYPE_DEBUG_LINE], "debug_line.vert");
        });

        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { LoadShader(ShaderStage::VS, shaders[VSTYPE_POSTPROCESS], "postprocess.vert"); });

        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::GS, shaders[GSTYPE_FLAT_SHADING], "flat_shader.geom"); });
        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::GS, shaders[GSTYPE_FLAT_DISNEY_SHADING], "flat_shader_disney.geom"); });
        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::GS, shaders[GSTYPE_FLAT_UNLIT], "flat_shader_unlit.geom"); });

        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::FS, shaders[FSTYPE_FLAT_SHADING], "flat_shader.frag"); });
        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::FS, shaders[FSTYPE_SKY], "sky.frag"); });
        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::FS, shaders[FSTYPE_POSTPROCESS_OUTLINE], "outline.frag"); });
        jobsystem::Execute(ctx, [](jobsystem::JobArgs) { LoadShader(ShaderStage::FS, shaders[FSTYPE_DEBUG_LINE], "debug_line.frag"); });

        jobsystem::Wait(ctx);

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
            dsd.depthFunc = ComparisonFunc::GreaterEqual;
            depth_stencils[DSSTYPE_DEPTH_READ] = dsd;

            dsd.depthEnable = true;
            dsd.stencilEnable = true;
            dsd.depthWriteMask = DepthWriteMask::All;
            dsd.depthFunc = ComparisonFunc::GreaterEqual;
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
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_SHADING);
            desc.fs = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            device->CreatePipelineState(&desc, &psoMaterial[MaterialComponent::Shadertype_BDRF]);
        }
        {
            // MaterialComponent::Shadertype_Disney_BDRF
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_DISNEY_SHADING);
            desc.fs = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            device->CreatePipelineState(&desc, &psoMaterial[MaterialComponent::Shadertype_Disney_BDRF]);
        }
        {
            // MaterialComponent::Shadertype_Unlit
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_FLAT_SHADING);
            desc.gs = GetShader(GSTYPE_FLAT_UNLIT);
            desc.fs = GetShader(FSTYPE_FLAT_SHADING);
            desc.rs = &rasterizers[RSTYPE_FRONT];
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
            desc.il = &input_layouts[VLTYPE_FLAT_SHADING];
            desc.pt = PrimitiveTopology::TriangleList;
            device->CreatePipelineState(&desc, &psoMaterial[MaterialComponent::Shadertype_Unlit]);
        }
        {
            // PSO_OUTLINE
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_POSTPROCESS);
            desc.fs = GetShader(FSTYPE_POSTPROCESS_OUTLINE);
            desc.rs = &rasterizers[RSTYPE_DOUBLESIDED];
            desc.dss = &depth_stencils[DSSTYPE_DEPTH_DISABLED];
            desc.pt = PrimitiveTopology::TriangleStrip;
            device->CreatePipelineState(&desc, &psoOutline);
        }
        {
            // PSO_SKY
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_SKY);
            desc.fs = GetShader(FSTYPE_SKY);
            desc.rs = &rasterizers[RSTYPE_SKY];
            desc.dss = &depth_stencils[DSSTYPE_SKY];
            desc.pt = PrimitiveTopology::TriangleStrip;
            device->CreatePipelineState(&desc, &psoSky);
        }
        {
            // DEBUGRENDERING_CUBE
            PipelineStateDesc desc;
            desc.vs = GetShader(VSTYPE_DEBUG_LINE);
            desc.fs = GetShader(FSTYPE_DEBUG_LINE);
            desc.rs = &rasterizers[RSTYPE_WIRE_DOUBLESIDED];
            desc.dss = &depth_stencils[DSSTYPE_DEPTH_READ];
            desc.il = &input_layouts[VLTYPE_DEBUG_LINE];
            desc.pt = PrimitiveTopology::LineList;
            device->CreatePipelineState(&desc, &pso_debug[DEBUGRENDERING_CUBE]);
        }

        jobsystem::Wait(ctx);
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

        jobsystem::Context ctx;
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
        frameCB.gamma = GAMMA;

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
        frameCB.pointLightsOffset = frameCB.numLights;
        for (uint32_t i = 0; i < frameCB.numLights; ++i)
        {
            if (i >= frameCB.pointLightsOffset)
                break;

            if (frameCB.lights[i].type == LIGHTSOURCE_TYPE_POINT)
            {
                std::exchange(frameCB.lights[i], frameCB.lights[frameCB.pointLightsOffset - 1]);
                frameCB.pointLightsOffset--;
            }
        }
    }

    void UpdateRenderData(const SceneView& view, const FrameConstants& frameCB, rhi::CommandList cmd)
    {
        device->BeginEvent("UpdateRenderData", cmd);
        device->UpdateBuffer(&constantbuffers[CBTYPE_FRAME], &frameCB, cmd);
        device->EndEvent(cmd);
    }

    void BindCameraCB(const scene::CameraComponent* camera, rhi::CommandList cmd)
    {
        assert(camera);

        CameraConstants cc = {};
        cc.proj = camera->projection;
        cc.view = camera->view;
        cc.vp = camera->VP;
        cc.inv_proj = camera->inv_projection;
        cc.inv_view = camera->inv_view;
        cc.inv_vp = camera->inv_VP;
        cc.pos = XMFLOAT4(camera->pos.x, camera->pos.y, camera->pos.z, 1.0f);

        device->UpdateBuffer(&constantbuffers[CBTYPE_CAMERA], &cc, cmd);
    }

    void DrawScene(const SceneView& view, CommandList cmd)
    {
        device->BeginEvent("DrawScene", cmd);

        device->BindConstantBuffer(&constantbuffers[CBTYPE_FRAME], CBSLOT_FRAME, cmd);
        device->BindConstantBuffer(&constantbuffers[CBTYPE_CAMERA], CBSLOT_CAMERA, cmd);

        uint8_t prevUserStencilRef = 0;
        device->BindStencilRef(0, cmd);

        // Draw all visiable objects
        for (uint32_t objectIndex : view.objectIndexes)
        {
            const ObjectComponent& object = view.scene->objects[objectIndex];

            if (object.userStencilRef != prevUserStencilRef)
            {
                prevUserStencilRef = object.userStencilRef;
                device->BindStencilRef(object.userStencilRef, cmd);
            }

            const MeshComponent& mesh = view.scene->meshes[object.meshIndex];
            if (mesh.vertex_buffer_col.IsValid())
            {
                std::array<const rhi::GPUBuffer*, 2> vertex_buffers = {
                    &mesh.vertex_buffer_pos,
                    &mesh.vertex_buffer_col
                };

                std::array<uint32_t, 2> strides = {
                    sizeof(scene::MeshComponent::Vertex_Pos),
                    sizeof(scene::MeshComponent::Vertex_Col)
                };

                device->BindVertexBuffers(vertex_buffers.data(), vertex_buffers.size(), strides.data(), nullptr, cmd);
                device->BindIndexBuffer(&mesh.index_buffer, IndexBufferFormat::Uint32, 0, cmd);
            }
            else
            {
                //device->BindVertexBuffer(&mesh->vertex_buffer_pos);
            }

            const TransformComponent& transform = view.scene->transforms[object.transformIndex];
            MiscCB cb = {};
            XMMATRIX W = XMLoadFloat4x4(&transform.world);
            XMStoreFloat4x4(&cb.g_xModelMatrix, XMMatrixTranspose(W));
            XMStoreFloat4x4(&cb.g_xTransform, XMMatrixTranspose(W * view.camera->GetViewProjection()));
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

                const PipelineState* pso = &psoMaterial[material.shaderType];
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
        device->BindPipelineState(&psoSky, cmd);

        device->BindConstantBuffer(&constantbuffers[CBTYPE_FRAME], CBSLOT_FRAME, cmd);
        device->BindConstantBuffer(&constantbuffers[CBTYPE_CAMERA], CBSLOT_CAMERA, cmd);

        device->Draw(3, 0, cmd);
        device->EndEvent(cmd);
    }

    void DrawDebugScene(const SceneView& view, CommandList cmd)
    {
        static GPUBuffer wirecube_vb;
        static GPUBuffer wirecube_ib;

        device->BeginEvent("DrawDebugScene", cmd);

        if (!wirecube_vb.IsValid())
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

            GPUBufferDesc vertexbuffer_desc;
            vertexbuffer_desc.usage = MemoryAccess::Default;
            vertexbuffer_desc.size = sizeof(verts);
            vertexbuffer_desc.bindFlags = BindFlags::VertexBufferBit;
            device->CreateBuffer(&vertexbuffer_desc, &verts, &wirecube_vb);

            const uint16_t indices[] = {
                0,1,1,2,0,3,0,4,1,5,4,5,
                5,6,4,7,2,6,3,7,2,3,6,7
            };

            GPUBufferDesc indexbuffer_desc;
            indexbuffer_desc.usage = MemoryAccess::Default;
            indexbuffer_desc.size = sizeof(indices);
            indexbuffer_desc.bindFlags = BindFlags::IndexBufferBit;
            device->CreateBuffer(&indexbuffer_desc, &indices, &wirecube_ib);
        }

        // Draw bounding boxes for all visible objects
        if (r_debugObjectAABB.GetValue())
        {
            device->BeginEvent("DebugObjectAABB", cmd);
            device->BindPipelineState(&pso_debug[DEBUGRENDERING_CUBE], cmd);

            const GPUBuffer* vbs[] = {
                &wirecube_vb,
            };
            const uint32_t strides[] = {
                sizeof(XMFLOAT4) + sizeof(XMFLOAT4),
            };
            device->BindVertexBuffers(vbs, _countof(vbs), strides, nullptr, cmd);
            device->BindIndexBuffer(&wirecube_ib, IndexBufferFormat::Uint16, 0, cmd);

            MaterialCB material_cb;
            material_cb.baseColor = XMFLOAT4(1.0f, 0.933f, 0.6f, 1.0f);
            device->BindDynamicConstantBuffer(material_cb, CBSLOT_MATERIAL, cmd);

            for (uint32_t objectIndex : view.objectIndexes)
            {
                const AxisAlignedBox& aabb = view.scene->aabb_objects[objectIndex];
                MiscCB misc_cb;
                XMStoreFloat4x4(&misc_cb.g_xTransform, XMMatrixTranspose(aabb.GetAsBoxMatrix() * view.camera->GetViewProjection()));
                device->BindDynamicConstantBuffer(misc_cb, CBSLOT_MISC, cmd);

                device->DrawIndexed(24, 0, 0, cmd);
            }
            device->EndEvent(cmd);
        }

        if (r_debugLightSources.GetValue())
        {
            device->BeginEvent("DebugLightsources", cmd);

            // Draw icons over all the lightsources
            for (uint32_t lightIndex : view.lightIndexes)
            {
                const ecs::Entity lightID = view.scene->lights.GetEntity(lightIndex);
                const scene::LightComponent& light = view.scene->lights[lightIndex];
                const scene::TransformComponent* transform = view.scene->transforms.GetComponent(lightID);

                float dist = math::Distance(transform->translation_local, view.camera->pos) * 0.05f;
                renderer::ImageParams params;
                params.EnableDepthTest();
                params.position = transform->translation_local;
                params.size = XMFLOAT2(dist, dist);

                XMMATRIX invR = XMMatrixInverse(nullptr, XMLoadFloat3x3(&view.camera->rotation));
                XMMATRIX P = view.camera->GetViewProjection();
                params.customRotation = &invR;
                params.customProjection = &P;

                device->BindSampler(GetSamplerState(renderer::SSLOT_BILINEAR_CLAMP), 0, cmd);

                switch (light.GetType())
                {
                case scene::LightType::Directional:
                    renderer::DrawImage(&builtin_textures[BUILTIN_TEXTURE_DIRLIGHT].GetTexture(), params, cmd);
                    break;
                case scene::LightType::Point:
                    renderer::DrawImage(&builtin_textures[BUILTIN_TEXTURE_POINTLIGHT].GetTexture(), params, cmd);
                    break;
                default:
                    break;
                }
            }

            // Draw all the aabb boxes for lightsources
            device->BindPipelineState(&pso_debug[DEBUGRENDERING_CUBE], cmd);
            const GPUBuffer* vbs[] = {
                &wirecube_vb,
            };
            const uint32_t strides[] = {
                sizeof(XMFLOAT4) + sizeof(XMFLOAT4),
            };
            device->BindVertexBuffers(vbs, _countof(vbs), strides, nullptr, cmd);
            device->BindIndexBuffer(&wirecube_ib, IndexBufferFormat::Uint16, 0, cmd);

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
                    XMStoreFloat4x4(&cbMisc.g_xTransform, XMMatrixTranspose(aabb.GetAsBoxMatrix() * view.camera->GetViewProjection()));
                    device->BindDynamicConstantBuffer(cbMisc, CBSLOT_MISC, cmd);
                    device->DrawIndexed(24, 0, 0, cmd);
                }
            }

            device->EndEvent(cmd);
        }

        device->EndEvent(cmd);
    }

    void Postprocess_Outline(
        const Texture& input,
        CommandList cmd,
        float thickness,
        float threshold,
        const XMFLOAT4& color)
    {
        device->BeginEvent("Postprocess_Outline", cmd);

        device->BindPipelineState(&psoOutline, cmd);
        device->BindSampler(&samplerStates[SSLOT_POINT_CLAMP], 0, cmd);
        device->BindResource(&input, 0, cmd);

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