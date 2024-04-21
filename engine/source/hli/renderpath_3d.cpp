#include "core/platform.h"
#include "core/profiler.h"
#include "graphics/device.h"
#include "systems/scene.h"
#include "hli/renderpath_3d.h"
#include "input/input.h"

using namespace cyb::graphics;

namespace cyb::hli
{
    void RenderPath3D::ResizeBuffers()
    {
        GraphicsDevice* device = cyb::graphics::GetDevice();
        XMUINT2 internalResolution = GetInternalResolution();

        // Render targets:
        {
            TextureDesc desc;
            desc.format = Format::R8G8B8A8_Unorm;
            desc.bindFlags = BindFlags::ShaderResourceBit | BindFlags::RenderTargetBit;
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            device->CreateTexture(&desc, nullptr, &renderTarget_Main);
            device->SetName(&renderTarget_Main, "renderTarget_Main");
        }

        // Depth stencil buffer:
        {
            TextureDesc desc;
            desc.layout = ResourceState::DepthStencil_ReadOnlyBit;
            desc.format = Format::D24_Float_S8_Uint;
            desc.bindFlags = BindFlags::DepthStencilBit;
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            device->CreateTexture(&desc, nullptr, &depthBuffer_Main);
            device->SetName(&depthBuffer_Main, "depthBuffer_Main");
        }

        RenderPath2D::ResizeBuffers();
    }

    void RenderPath3D::Update(double dt)
    {
        RenderPath2D::Update(dt);

        runtime += dt;

        scene->Update(dt);
        camera->TransformCamera(cameraTransform);
        camera->UpdateCamera();

        // Update the main view:
        sceneViewMain.Clear();
        sceneViewMain.Update(scene, camera);

        // Update per frame constant buffer
        renderer::UpdatePerFrameData(sceneViewMain, static_cast<float>(runtime), frameCB);

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

        const RenderPassImage renderPassImages[] = {
                    RenderPassImage::RenderTarget(
                        &renderTarget_Main,
                        RenderPassImage::LoadOp::DontCare),
                    RenderPassImage::DepthStencil(
                        &depthBuffer_Main,
                        RenderPassImage::LoadOp::Clear,
                        RenderPassImage::StoreOp::Store
                    )
        };
        device->BeginRenderPass(renderPassImages, _countof(renderPassImages), cmd);

        {
            CYB_PROFILE_GPU_SCOPE("Opaque Scene", cmd);
            renderer::DrawScene(sceneViewMain, cmd);
            renderer::DrawSky(sceneViewMain.camera, cmd);
            device->EndEvent(cmd);
        }

        {
            CYB_PROFILE_GPU_SCOPE("Debug Scene", cmd);
            renderer::DrawDebugScene(sceneViewMain, cmd);
            device->EndEvent(cmd);
        }

        device->EndRenderPass(cmd);
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
