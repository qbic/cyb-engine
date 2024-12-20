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

    public:
        scene::CameraComponent* camera = &cyb::scene::GetCamera();
        scene::TransformComponent cameraTransform;
        scene::Scene* scene = &cyb::scene::GetScene();
        renderer::SceneView sceneViewMain;
        //renderer::SceneView sceneViewDebug;

        FrameCB frameCB = {};

        graphics::Texture renderTarget_Main;
        graphics::Texture depthBuffer_Main;

        double runtime = 0.0;                       // Accumilated delta times
    };
}