#include "core/platform.h"
#include "core/profiler.h"
#include "graphics/graphics-device.h"
#include "systems/scene.h"
#include "hli/render-path-3d.h"
#include "input/input.h"

using namespace cyb::graphics;

namespace cyb::hli
{
    void RenderPath3D::ResizeBuffers()
    {
        GraphicsDevice* device = cyb::graphics::GetDevice();
        XMUINT2 internal_resolution = GetInternalResolution();

        // Render targets:
        {
            TextureDesc desc;
            desc.format = Format::R8G8B8A8_Unorm;
            desc.bindFlags = BindFlags::ShaderResourceBit | BindFlags::RenderTargetBit;
            desc.width = internal_resolution.x;
            desc.height = internal_resolution.y;
            device->CreateTexture(&desc, nullptr, &renderTarget_Main);
            device->SetName(&renderTarget_Main, "renderTarget_Main");
        }

        depthBuffer_Main.internal_state = std::make_shared<int>();

        // Depth stencil buffer:
        {
            TextureDesc desc;
            desc.format = Format::D32_Float_S8_Uint;
            desc.bindFlags = BindFlags::ShaderResourceBit | BindFlags::DepthStencilBit;
            desc.layout = ResourceState::DepthStencil_ReadOnlyBit;
            desc.width = internal_resolution.x;
            desc.height = internal_resolution.y;
            device->CreateTexture(&desc, nullptr, &depthBuffer_Main);
            device->SetName(&depthBuffer_Main, "depthBuffer_Main");
        }

        // Render pass:
        {
            RenderPassDesc desc;
            desc.attachments.push_back(
                RenderPassAttachment::RenderTarget(
                    &renderTarget_Main,
                    RenderPassAttachment::LoadOp::DontCare));
            desc.attachments.push_back(
                RenderPassAttachment::DepthStencil(
                    &depthBuffer_Main,
                    RenderPassAttachment::LoadOp::Clear,
                    RenderPassAttachment::StoreOp::Store,
                    ResourceState::DepthStencil_ReadOnlyBit,
                    ResourceState::DepthStencilBit,
                    ResourceState::DepthStencil_ReadOnlyBit));

            device->CreateRenderPass(&desc, &renderPass_Main);
        }

        RenderPath2D::ResizeBuffers();
    }

    void RenderPath3D::UpdateViewport() const
    {
        const XMINT2 client_size = cyb::platform::main_window->GetClientSize();
    }

    void RenderPath3D::Update(float dt)
    {
        runtime += dt;

        UpdateViewport();

        scene->Update(dt);
        camera->TransformCamera(camera_transform);
        camera->UpdateCamera();

        // Update the main view:
        sceneViewMain.Clear();
        sceneViewMain.Update(scene, camera);

        // Update per frame constant buffer
        renderer::UpdatePerFrameData(sceneViewMain, runtime, frameCB);

        RenderPath2D::Update(dt);
    }

    void RenderPath3D::Render() const
    {
        auto device = cyb::graphics::GetDevice();

        // Prepare the frame:
        auto cmd = device->BeginCommandList();
        renderer::BindCameraCB(camera, cmd);
        renderer::UpdateRenderData(sceneViewMain, frameCB, cmd);

        device->BeginEvent("Opaque Scene", cmd);
        Viewport viewport;
        viewport.width = (float)renderTarget_Main.GetDesc().width;
        viewport.height = (float)renderTarget_Main.GetDesc().height;
        device->BindViewports(&viewport, 1, cmd);

        {
            CYB_PROFILE_GPU_SCOPE("Opaque Scene", cmd);
            device->BeginRenderPass(&renderPass_Main, cmd);
            renderer::DrawScene(sceneViewMain, cmd);
            renderer::DrawSky(sceneViewMain.camera, cmd);
            device->EndEvent(cmd);
        }

        {
            CYB_PROFILE_GPU_SCOPE("Debug Scene", cmd);
            renderer::DrawDebugScene(sceneViewMain, cmd);
            device->EndEvent(cmd);
            device->EndRenderPass(cmd);
        }

        RenderPath2D::Render();
    }

    void RenderPath3D::Compose(CommandList cmd) const
    {
        GraphicsDevice* device = graphics::GetDevice();
        renderer::ImageParams params;
        params.fullscreen = true;

        device->BeginEvent("Composition", cmd);
        const graphics::Sampler* pointerSampler = renderer::GetSamplerState(renderer::SSLOT_POINT_WRAP);
        device->BindSampler(pointerSampler, 0, cmd);
        renderer::DrawImage(&renderTarget_Main, params, cmd);
        device->EndEvent(cmd);

        RenderPath2D::Compose(cmd);
    }
}
