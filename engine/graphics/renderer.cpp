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

using namespace cyb::graphics;
using namespace cyb::scene;

namespace cyb::renderer
{
    GraphicsDevice*& device = GetDevice();

    CVar r_debugObjectAABB("r_debugObjectAABB", false, CVarFlag::RendererBit, "Render AABB of all objects in the scene");
    CVar r_debugLightSources("r_debugLightSources", false, CVarFlag::RendererBit, "Render icon and AABB of all light sources");
    
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

    std::string SHADERPATH = "../engine/shaders/";

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

    const graphics::RasterizerState* GetRasterizerState(RSTYPES id)
    {
        assert(id < RSTYPE_COUNT);
        return &rasterizers[id];
    }

    const graphics::DepthStencilState* GetDepthStencilState(DSSTYPES id)
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
            desc.size = sizeof(FrameCB);
            desc.stride = 0;
            device->CreateBuffer(&desc, nullptr, &constantbuffers[CBTYPE_FRAME]);
            device->SetName(&constantbuffers[CBTYPE_FRAME], "constantbuffers[CBTYPE_FRAME]");

            desc.size = sizeof(CameraCB);
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

    void SetShaderPath(const std::string& path)
    {
        SHADERPATH = path;
    }

    const std::string& GetShaderPath()
    {
        return SHADERPATH;
    }

    bool LoadShader(ShaderStage stage, Shader& shader, const std::string& filename)
    {
        const std::string fullPath = SHADERPATH + filename;
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
            dsd.stencilEnable = true;
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
            desc.dss = &depth_stencils[DSSTYPE_DEFAULT];
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
        GetCamera().CreatePerspective(1.78f, 0.1f, 1000.0f, 70.0f);

        jobsystem::Context ctx;
        LoadBuiltinTextures(ctx);
        LoadBuffers(ctx);
        LoadShaders();
        LoadSamplerStates();
        Image_Initialize();

        jobsystem::Wait(ctx);

        static eventsystem::Handle handle = eventsystem::Subscribe(eventsystem::Event_ReloadShaders, [] (uint64_t userdata) { LoadShaders(); });
    }

    void SceneView::Clear()
    {
        visibleObjects.clear();
        visibleLights.clear();
    }

    void SceneView::Update(const scene::Scene* scene, const scene::CameraComponent* camera)
    {
        assert(scene);
        assert(camera);
        assert(visibleObjects.empty());
        assert(visibleLights.empty());

        this->scene = scene;
        this->camera = camera;

        {
            CYB_PROFILE_CPU_SCOPE("Frustum Culling");
            // Perform camera frustum culling to all objects aabb and
            // store all visible objects in the view
            const spatial::Frustum& frustum = camera->frustum;
            visibleObjects.resize(scene->aabb_objects.Size());
            size_t visibleObjectsCount = 0;
            for (size_t i = 0; i < scene->aabb_objects.Size(); ++i)
            {
                const spatial::AxisAlignedBox& aabb = scene->aabb_objects[i];
                const scene::ObjectComponent& object = scene->objects[i];
                if (HasFlag(object.flags, scene::ObjectComponent::Flags::RenderableBit) &&
                    frustum.IntersectsBoundingBox(aabb))
                {
                    visibleObjects[visibleObjectsCount] = static_cast<uint32_t>(i);
                    visibleObjectsCount++;
                }
            }
            visibleObjects.resize(visibleObjectsCount);

            // TODO: Perform aabb light culling
        }
    }

    void UpdatePerFrameData(const SceneView& view, float time, FrameCB& frameCB)
    {
        frameCB.time = time;
        frameCB.gamma = GAMMA;

        // Setup weather
        const scene::WeatherComponent& weather = view.scene->active_weather;
        frameCB.horizon = weather.horizon;
        frameCB.zenith = weather.zenith;
        frameCB.drawSun = weather.drawSun;
        frameCB.mostImportantLightIndex = 0;
        frameCB.fog = XMFLOAT4(weather.fogStart, weather.fogEnd, weather.fogHeight, 1.0f / (weather.fogEnd - weather.fogStart));
        frameCB.cloudiness = weather.cloudiness;
        frameCB.cloudTurbulence = weather.cloudTurbulence;
        frameCB.cloudHeight = weather.cloudHeight;
        frameCB.windSpeed = weather.windSpeed;

        // Setup lightsources
        frameCB.numLights = 0;
        float brightestLight = 0.0f;
        for (size_t i = 0; i < view.scene->lights.Size(); ++i)
        {
            const ecs::Entity lightID = view.scene->lights.GetEntity(i);
            const scene::LightComponent* light = view.scene->lights.GetComponent(lightID);
            const scene::TransformComponent* transform = view.scene->transforms.GetComponent(lightID);

            if (light->IsAffectingScene())
            {
                LightSource& cbLight = frameCB.lights[frameCB.numLights];
                cbLight.type = static_cast<uint32_t>(light->GetType());
                cbLight.position = XMFLOAT4(transform->translation_local.x, transform->translation_local.y, transform->translation_local.z, 0.0f);
                cbLight.direction = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                cbLight.color = XMFLOAT4(light->color.x, light->color.y, light->color.z, 0.0f);
                cbLight.energy = light->energy;
                cbLight.range = light->range;
                ++frameCB.numLights;

                // most importand light will be used for placing the sun
                if (light->GetType() == scene::LightType::Directional && light->energy > brightestLight)
                {
                    frameCB.mostImportantLightIndex = (int)i;
                    brightestLight = light->energy;
                }
            }
        }
    }

    void UpdateRenderData(const SceneView& view, const FrameCB& frameCB, graphics::CommandList cmd)
    {
        device->BeginEvent("UpdateRenderData", cmd);
        device->UpdateBuffer(&constantbuffers[CBTYPE_FRAME], &frameCB, cmd);
        device->EndEvent(cmd);
    }

    void BindCameraCB(const scene::CameraComponent* camera, graphics::CommandList cmd)
    {
        assert(camera);

        CameraCB cameraCB = {};
        cameraCB.proj = camera->projection;
        cameraCB.view = camera->view;
        cameraCB.vp = camera->VP;
        cameraCB.inv_proj = camera->inv_projection;
        cameraCB.inv_view = camera->inv_view;
        cameraCB.inv_vp = camera->inv_VP;
        cameraCB.pos = XMFLOAT4(camera->pos.x, camera->pos.y, camera->pos.z, 1.0f);

        device->UpdateBuffer(&constantbuffers[CBTYPE_CAMERA], &cameraCB, cmd);
    }

    void DrawScene(const SceneView& view, CommandList cmd)
    {
        device->BeginEvent("DrawScene", cmd);

        device->BindConstantBuffer(&constantbuffers[CBTYPE_FRAME], CBSLOT_FRAME, cmd);
        device->BindConstantBuffer(&constantbuffers[CBTYPE_CAMERA], CBSLOT_CAMERA, cmd);

        uint8_t prevUserStencilRef = 0;

        // Draw all visiable objects
        for (uint32_t instanceIndex : view.visibleObjects)
        {
            const ObjectComponent& object = view.scene->objects[instanceIndex];

            if (object.userStencilRef != prevUserStencilRef)
            {
                prevUserStencilRef = object.userStencilRef;
                device->BindStencilRef(object.userStencilRef, cmd);
            }

            if (object.meshID != ecs::INVALID_ENTITY)
            {
                const MeshComponent* mesh = view.scene->meshes.GetComponent(object.meshID);
                if (mesh != nullptr)
                {
                    if (mesh->vertex_buffer_col.IsValid())
                    {
                        const graphics::GPUBuffer* vertex_buffers[] = {
                            &mesh->vertex_buffer_pos,
                            &mesh->vertex_buffer_col
                        };

                        const uint32_t strides[] = {
                            sizeof(scene::MeshComponent::Vertex_Pos),
                            sizeof(scene::MeshComponent::Vertex_Col)
                        };

                        device->BindVertexBuffers(vertex_buffers, (uint32_t)CountOf(vertex_buffers), strides, nullptr, cmd);
                        device->BindIndexBuffer(&mesh->index_buffer, IndexBufferFormat::Uint32, 0, cmd);
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

                    for (const auto& subset : mesh->subsets)
                    {
                        // Setup Object constant buffer
                        const MaterialComponent* material = view.scene->materials.GetComponent(subset.materialID);
                        MaterialCB material_cb;
                        material_cb.baseColor = material->baseColor;
                        material_cb.roughness = material->roughness;
                        material_cb.metalness = material->metalness;
                        device->BindDynamicConstantBuffer(material_cb, CBSLOT_MATERIAL, cmd);

                        const PipelineState* pso = &psoMaterial[material->shaderType];
                        device->BindPipelineState(pso, cmd);
                        device->DrawIndexed(subset.indexCount, subset.indexOffset, 0, cmd);
                    }
                }
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
        if (r_debugObjectAABB.GetValue<bool>())
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

            for (uint32_t instanceIndex : view.visibleObjects)
            {
                const spatial::AxisAlignedBox& aabb = view.scene->aabb_objects[instanceIndex];
                MiscCB misc_cb;
                XMStoreFloat4x4(&misc_cb.g_xTransform, XMMatrixTranspose(aabb.GetAsBoxMatrix() * view.camera->GetViewProjection()));
                device->BindDynamicConstantBuffer(misc_cb, CBSLOT_MISC, cmd);

                device->DrawIndexed(24, 0, 0, cmd);
            }
            device->EndEvent(cmd);
        }

        if (r_debugLightSources.GetValue<bool>())
        {
            device->BeginEvent("DebugLightsources", cmd);
            const scene::Scene& scene = *view.scene;

            // Draw icons over all the lightsources
            for (uint32_t i = 0; i < scene.lights.Size(); ++i)
            {
                const ecs::Entity lightID = scene.lights.GetEntity(i);
                const scene::LightComponent* light = scene.lights.GetComponent(lightID);
                const scene::TransformComponent* transform = scene.transforms.GetComponent(lightID);

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

                switch (light->GetType())
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

            for (uint32_t i = 0; i < scene.aabb_lights.Size(); ++i)
            {
                ecs::Entity entity = scene.aabb_lights.GetEntity(i);
                const LightComponent* light = scene.lights.GetComponent(entity);

                if (light->type == LightType::Point)
                {
                    const spatial::AxisAlignedBox& aabb = scene.aabb_lights[i];
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
        float threshold,
        float thickness,
        const XMFLOAT4& color)
    {
        device->BeginEvent("Postprocess_Outline", cmd);

        device->BindPipelineState(&psoOutline, cmd);
        device->BindSampler(&samplerStates[SSLOT_POINT_CLAMP], 0, cmd);
        device->BindResource(&input, 0, cmd);

        PostProcess postprocess = {};
        postprocess.param0.x = threshold;
        postprocess.param0.y = thickness;
        postprocess.param0.z = input.GetDesc().width;
        postprocess.param0.w = input.GetDesc().height;
        postprocess.param1.x = color.x;
        postprocess.param1.y = color.y;
        postprocess.param1.z = color.z;
        postprocess.param1.w = color.w;
        device->PushConstants(&postprocess, sizeof(postprocess), cmd);
        device->Draw(3, 0, cmd);

        device->EndEvent(cmd);
    }
}