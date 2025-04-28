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
        void Compose(graphics::CommandList cmd) const override;

        graphics::Rect GetScissorInternalResolution() const
        {
            graphics::Rect scissor;
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

        graphics::Texture renderTarget_Main;
        graphics::Texture depthBuffer_Main;
        graphics::Texture rtSelectionOutline;

        double runtime = 0.0;                       // Accumilated delta times
    };
}