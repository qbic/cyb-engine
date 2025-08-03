#pragma once
#include "core/intersect.h"
#include "systems/resource_manager.h"
#include "graphics/device.h"
#include "../shaders/shader_interop.h"

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
        VSTYPE_POSTPROCESS,
        VSTYPE_SKY,
        VSTYPE_DEBUG_LINE,

        // Fragment shaders
        FSTYPE_FLAT_SHADING,
        FSTYPE_POSTPROCESS_OUTLINE,
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
        RSTYPE_SKY,
        RSTYPE_COUNT
    };

    // depth-stencil states
    enum DSSTYPES
    {
        DSSTYPE_DEFAULT,
        DSSTYPE_SKY,
        DSSTYPE_DEPTH_READ,
        DSSTYPE_DEPTH_DISABLED,
        DSSTYPE_COUNT
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

    // Contains a fully clipped view of the scene from the camera perspective
    struct SceneView
    {
        void Reset(const scene::Scene* scene, const scene::CameraComponent* camera);

        const scene::Scene* scene = nullptr;
        const scene::CameraComponent* camera = nullptr;

        uint32_t objectCount = 0;
        uint32_t lightCount = 0;
        std::vector<uint32_t> objectIndexes;   // scene->objects indexes
        std::vector<uint32_t> lightIndexes;    // scene->lights indexes
    };

    const rhi::Shader* GetShader(SHADERTYPE id);
    const rhi::Sampler* GetSamplerState(SSLOT id);
    const rhi::RasterizerState* GetRasterizerState(RSTYPES id);
    const rhi::DepthStencilState* GetDepthStencilState(DSSTYPES id);

    void Initialize();
    void ReloadShaders();
    bool LoadShader(rhi::ShaderType stage, rhi::Shader& shader, const std::string& filename);

    // Prepare view for rendering
    void UpdatePerFrameData(const SceneView& view, float time, FrameConstants& frameCB);

    // Updates the GPU state according to the previously called UpdatePerFrameData()
    void UpdateRenderData(const SceneView& view, const FrameConstants& frameCB, rhi::CommandList cmd);

    // Updated the per camera constant buffer
    void BindCameraCB(const scene::CameraComponent* camera, rhi::CommandList cmd);

    // Draw the scene from a RenderView
    void DrawScene(const SceneView& view, rhi::CommandList cmd);

    // Draw skydome centered at the camera
    void DrawSky(const scene::CameraComponent* scene, rhi::CommandList cmd);

    // Draw debug primitives according to the debug states
    void DrawDebugScene(const SceneView& view, rhi::CommandList cmd);

    void Postprocess_Outline(
        const rhi::Texture& input,
        rhi::CommandList cmd,
        float thickness,
        float threshold,
        const XMFLOAT4& color);
}