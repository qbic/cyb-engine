#pragma once
#include "hli/render-path-2d.h"
#include "Graphics/Renderer.h"
#include "Systems/Scene.h"

namespace cyb::hli
{
    class RenderPath3D : public RenderPath2D
    {
    public:
        scene::CameraComponent* camera = &cyb::scene::GetCamera();
        scene::TransformComponent camera_transform;
        scene::Scene* scene = &cyb::scene::GetScene();
        renderer::SceneView sceneViewMain;
        //renderer::SceneView sceneViewDebug;

        FrameCB frameCB = {};

        graphics::Texture renderTarget_Main;
        graphics::Texture depthBuffer_Main;
        graphics::RenderPass renderPass_Main;

        float runtime = 0.0f;                       // Accumilated delta times

    public:
        void ResizeBuffers() override;
        void UpdateViewport() const;

        void Update(float dt) override;
        void Render() const override;
        void Compose(graphics::CommandList cmd) const override;
    };
}