#pragma once
#include "hli/renderpath_2d.h"
#include "graphics/renderer.h"
#include "systems/scene.h"

namespace cyb::hli
{
    class RenderPath3D : public RenderPath2D
    {
    public:
        void ResizeBuffers() override;
        void Update(double dt) override;
        void Render() const override;
        void Compose(rhi::CommandList cmd) const override;

        rhi::Rect GetScissorInternalResolution() const
        {
            rhi::Rect scissor;
            scissor.left = int(0);
            scissor.top = int(0);
            scissor.right = int(GetInternalResolution().x);
            scissor.bottom = int(GetInternalResolution().y);
            return scissor;
        }

    public:
        scene::CameraComponent* camera = &cyb::scene::GetCamera();
        scene::TransformComponent cameraTransform;
        scene::Scene* scene = &cyb::scene::GetScene();
        renderer::SceneView sceneViewMain;
        //renderer::SceneView sceneViewDebug;

        renderer::FrameConstants frameCB = {};

        rhi::Texture renderTarget_Main;
        rhi::Texture depthBuffer_Main;
        rhi::Texture rtSelectionOutline;

        double runtime = 0.0;                       // Accumilated delta times
    };
}