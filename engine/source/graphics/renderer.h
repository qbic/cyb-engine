#pragma once
#include "core/intersect.h"
#include "systems/resource_manager.h"
#include "graphics/device.h"
#include "../../shaders/shader_interop.h"

namespace cyb::scene 
{
    struct CameraComponent;
    struct Scene;
}

namespace cyb::renderer
{
    enum CBTYPE
    {
        CBTYPE_FRAME,
        CBTYPE_CAMERA,
        CBTYPE_MATERIAL,
        CBTYPE_IMAGE,
        CBTYPE_MISC,
        CBTYPE_COUNT
    };

    enum SHADERTYPE
    {
        // Vertex shaders
        VSTYPE_FLAT_SHADING,
        VSTYPE_IMAGE,
        VSTYPE_SKY,
        VSTYPE_DEBUG_LINE,

        // Fragment shaders
        FSTYPE_FLAT_SHADING,
        FSTYPE_IMAGE,
        FSTYPE_SKY,
        FSTYPE_DEBUG_LINE,

        // Geometry shaders
        GSTYPE_FLAT_SHADING,
        GSTYPE_FLAT_DISNEY_SHADING,
        GSTYPE_FLAT_UNLIT,

        SHADERTYPE_COUNT
    };

    enum VLTYPE
    {
        VLTYPE_NULL,
        VLTYPE_FLAT_SHADING,
        VLTYPE_SKY,
        VLTYPE_DEBUG_LINE,
        VLTYPE_COUNT
    };

    // rasterizer states
    enum RSTYPES
    {
        RSTYPE_FRONT,
        RSTYPE_BACK,
        RSTYPE_DOUBLESIDED,
        RSTYPE_WIRE,
        RSTYPE_WIRE_DOUBLESIDED,
        RSTYPE_IMAGE,
        RSTYPE_COUNT
    };

    // depth-stencil states
    enum DSSTYPES
    {
        DSSTYPE_DEFAULT,
        DSSTYPE_SKY,
        DSSTYPE_COUNT
    };

    // Pipeline states
    enum PSO_TYPE
    {
        PSO_NULL,
        PSO_FLAT_SHADING,
        PSO_FLAT_DISNEY_SHADING,
        PSO_IMAGE,
        PSO_SKY,
        PSO_DEBUG_CUBE,
        PSO_COUNT
    };

    enum SSLOT
    {
        SSLOT_POINT_WRAP,
        SSLOT_POINT_MIRROR,
        SSLOT_POINT_CLAMP,

        SSLOT_BILINEAR_WRAP,
        SSLOT_BILINEAR_MIRROR,
        SSLOT_BILINEAR_CLAMP,

        SSLOT_TRILINEAR_WRAP,
        SSLOT_TRILINEAR_MIRROR,
        SSLOT_TRILINEAR_CLAMP,

        SSLOT_ANISO_WRAP,
        SSLOT_ANISO_MIRROR,
        SSLOT_ANISO_CLAMP,
        SSLOT_COUNT
    };

    struct ImageParams
    {
        bool fullscreen = false;
        XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT2 size = XMFLOAT2(1.0f, 1.0f);
        XMFLOAT2 pivot = XMFLOAT2(0.5f, 0.5f);          // (0,0) : upperleft, (0.5,0.5) : center, (1,1) : bottomright
        XMFLOAT2 corners[4] = {
            XMFLOAT2(0.0f, 0.0f),
            XMFLOAT2(1.0f, 0.0f),
            XMFLOAT2(0.0f, 1.0f),
            XMFLOAT2(1.0f, 1.0f)
        };
    };

    // Contains a fully clipped view of the scene from the camera perspective
    struct SceneView
    {
        const scene::Scene* scene = nullptr;
        const scene::CameraComponent* camera = nullptr;
        std::vector<uint32_t> visibleObjects;   // scene->objects indexes
        std::vector<uint32_t> visibleLights;    // scene->lights indexes

        void Clear();
        void Update(const scene::Scene* scene, const scene::CameraComponent* camera);
    };

    const graphics::Shader* GetShader(SHADERTYPE id);
    const graphics::Sampler* GetSamplerState(SSLOT id);
    const graphics::RasterizerState* GetRasterizerState(RSTYPES id);
    const graphics::DepthStencilState* GetDepthStencilState(DSSTYPES id);

    void Initialize();
    void SetShaderPath(const std::string& path);
    const std::string& GetShaderPath();
    void ReloadShaders();
    bool LoadShader(graphics::ShaderStage stage, graphics::Shader& shader, const std::string& filename);

    // Prepare view for rendering
    void UpdatePerFrameData(const SceneView& view, float time, FrameCB& frameCB);

    // Updates the GPU state according to the previously called UpdatePerFrameData()
    void UpdateRenderData(const SceneView& view, const FrameCB& frameCB, graphics::CommandList cmd);

    // Updated the per camera constant buffer
    void BindCameraCB(const scene::CameraComponent* camera, graphics::CommandList cmd);

    // Draw the scene from a RenderView
    void DrawScene(const SceneView& view, graphics::CommandList cmd);

    // Draw skydome centered at the camera
    void DrawSky(const scene::CameraComponent* scene, graphics::CommandList cmd);

    // Set/Get state for DrawDebugScene()
    bool GetDebugObjectAABB();
    void SetDebugObjectAABB(bool value);
    bool GetDebugLightsources();
    void SetDebugLightsources(bool value);

    // Draw debug primitives according to the debug states
    void DrawDebugScene(const SceneView& view, graphics::CommandList cmd);

    void DrawImage(const graphics::Texture* texture, ImageParams& params, graphics::CommandList cmd);
}