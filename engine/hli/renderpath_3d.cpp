#include "core/platform.h"
#include "core/cvar.h"
#include "graphics/device.h"
#include "graphics/image.h"
#include "systems/profiler.h"
#include "systems/scene.h"
#include "hli/renderpath_3d.h"

#include "editor//editor.h"

using namespace cyb::rhi;

namespace cyb::hli
{
    CVar r_selectionOutlineThickness("r_selectionOutlineThickness", 1.5f, CVarFlag::RendererBit, "Thickness of selection outline");
    void RenderPath3D::ResizeBuffers()
    {
        GraphicsDevice* device = cyb::rhi::GetDevice();
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
            desc.bindFlags = BindFlags::DepthStencilBit | BindFlags::ShaderResourceBit;
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            device->CreateTexture(&desc, nullptr, &depthBuffer_Main);
            device->SetName(&depthBuffer_Main, "depthBuffer_Main");
        }

        // Selection outline
        {
            TextureDesc desc;
            desc.format = Format::R8_Unorm;
            desc.bindFlags = BindFlags::RenderTargetBit | BindFlags::ShaderResourceBit;
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            device->CreateTexture(&desc, nullptr, &rtSelectionOutline);
            device->SetName(&rtSelectionOutline, "rtSelectionOutline");
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
        sceneViewMain.Reset(scene, camera);

        // Update per frame constant buffer
        renderer::UpdatePerFrameData(sceneViewMain, static_cast<float>(runtime), frameCB);
    }

    void RenderPath3D::Render() const
    {
        auto device = cyb::rhi::GetDevice();

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
                RenderPassImage::StoreOp::Store)
        };
        device->BeginRenderPass(renderPassImages, _countof(renderPassImages), cmd);

        Rect scissor = GetScissorInternalResolution();
        device->BindScissorRects(&scissor, 1, cmd);

        {
            CYB_PROFILE_GPU_SCOPE("Opaque Scene", cmd);
            renderer::DrawScene(sceneViewMain, cmd);
            renderer::DrawSky(sceneViewMain.camera, cmd);
        }

        {
            CYB_PROFILE_GPU_SCOPE("Debug Scene", cmd);
            renderer::DrawDebugScene(sceneViewMain, cmd);
        }

        device->EndEvent(cmd);
        device->EndRenderPass(cmd);

#if 1
        device->BeginEvent("Selection Outline", cmd);
        {
            CYB_PROFILE_GPU_SCOPE("Selection Outline", cmd);
            const RenderPassImage rpStencilFill[] = {
                RenderPassImage::RenderTarget(
                    &rtSelectionOutline,
                    RenderPassImage::LoadOp::Clear),
                RenderPassImage::DepthStencil(
                    &depthBuffer_Main,
                    RenderPassImage::LoadOp::Load)
            };
            device->BeginRenderPass(rpStencilFill, _countof(rpStencilFill), cmd);

            renderer::ImageParams image = {};
            image.EnableFullscreen();
            image.stencilRef = 8;
            image.stencilComp = renderer::STENCILMODE_EQUAL;
            device->BindSampler(GetSamplerState(renderer::SSLOT_POINT_CLAMP), 0, cmd);
            renderer::DrawImage(nullptr, image, cmd);

            device->EndRenderPass(cmd);

            const RenderPassImage rpOutline[] = {
                RenderPassImage::RenderTarget(
                    &renderTarget_Main,
                    RenderPassImage::LoadOp::Load)
            };
            device->BeginRenderPass(rpOutline, _countof(rpOutline), cmd);
            XMFLOAT4 outlineColor = XMFLOAT4(1.0f, 0.62f, 0.17f, 1.0f);
            renderer::Postprocess_Outline(rtSelectionOutline, cmd, r_selectionOutlineThickness.GetValue<float>(), 0.05f, outlineColor);
            device->EndRenderPass(cmd);
        }
        device->EndEvent(cmd);
#endif
        RenderPath2D::Render();
    }

    void RenderPath3D::Compose(CommandList cmd) const
    {
        GraphicsDevice* device = rhi::GetDevice();
        renderer::ImageParams params = {};
        params.EnableFullscreen();

        device->BeginEvent("Composition", cmd);
        const rhi::Sampler* pointSampler = GetSamplerState(renderer::SSLOT_POINT_CLAMP);
        device->BindSampler(pointSampler, 0, cmd);
        renderer::DrawImage(&renderTarget_Main, params, cmd);
        device->EndEvent(cmd);

        RenderPath2D::Compose(cmd);
    }
}
