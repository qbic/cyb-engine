#include "core/sys.h"
#include "core/cvar.h"
#include "graphics/device.h"
#include "graphics/image.h"
#include "systems/profiler.h"
#include "systems/scene.h"
#include "hli/renderpath_3d.h"

using namespace cyb::rhi;

namespace cyb::hli
{
    CVar<float> r_selectionOutlineThickness("r_selectionOutlineThickness", 1.5f, CVarFlag::RendererBit, "Thickness of selection outline");
    
    void RenderPath3D::ResizeBuffers()
    {
        GraphicsDevice* device = cyb::rhi::GetDevice();
        UVec2 internalResolution = GetInternalResolution();

        // Render targets:
        {
            TextureDesc desc{};
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            desc.format = Format::RGBA8_UNORM;
            desc.initialState = ResourceStates::ShaderResourceBit | ResourceStates::RenderTargetBit;
			desc.debugName = "rtMain";
            rtMain = device->CreateTexture(&desc, nullptr);
        }

        // Depth stencil buffer:
        {
            TextureDesc desc{};
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            desc.format = Format::D24S8;
            desc.initialState = ResourceStates::DepthWriteBit;
            desc.debugName = "rtMainDepth";
            rtMainDepth = device->CreateTexture(&desc, nullptr);
        }

        // Selection outline
        {
            TextureDesc desc{};
            desc.width = internalResolution.x;
            desc.height = internalResolution.y;
            desc.format = Format::R8_UNORM;
            desc.initialState = ResourceStates::ShaderResourceBit | ResourceStates::RenderTargetBit;
            desc.debugName = "rtSelectionOutline";
            rtSelectionOutline = device->CreateTexture(&desc, nullptr);
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
        viewport.width = (float)rtMain->GetDesc().width;
        viewport.height = (float)rtMain->GetDesc().height;
        device->BindViewports(&viewport, 1, cmd);

        const std::array renderPassImages = std::to_array<RenderPassImage>({
            RenderPassImage::RenderTarget(
                rtMain,
                RenderPassImage::LoadOp::DontCare),
            RenderPassImage::DepthStencil(
                rtMainDepth,
                RenderPassImage::LoadOp::Clear,
                RenderPassImage::StoreOp::Store)
        });
        device->BeginRenderPass(renderPassImages.data(), renderPassImages.size(), cmd);

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
            const std::array rpStencilFill = std::to_array<RenderPassImage>({
                RenderPassImage::RenderTarget(
                    rtSelectionOutline,
                    RenderPassImage::LoadOp::Clear),
                RenderPassImage::DepthStencil(
                    rtMainDepth,
                    RenderPassImage::LoadOp::Load)
            });
            device->BeginRenderPass(rpStencilFill.data(), rpStencilFill.size(), cmd);

            renderer::ImageParams image{};
            image.EnableFullscreen();
            image.stencilRef = 8;
            image.stencilComp = renderer::STENCILMODE_EQUAL;
            device->BindSampler(GetSamplerState(renderer::SSLOT_POINT_CLAMP), 0, cmd);
            renderer::DrawImage(nullptr, image, cmd);

            device->EndRenderPass(cmd);

            const std::array rpOutline = std::to_array<RenderPassImage>({
                RenderPassImage::RenderTarget(
                    rtMain,
                    RenderPassImage::LoadOp::Load)
            });
            device->BeginRenderPass(rpOutline.data(), rpOutline.size(), cmd);
            XMFLOAT4 outlineColor = XMFLOAT4(1.0f, 0.62f, 0.17f, 1.0f);
            renderer::Postprocess_Outline(rtSelectionOutline, cmd, r_selectionOutlineThickness.GetValue(), 0.05f, outlineColor);
            device->EndRenderPass(cmd);
        }
        device->EndEvent(cmd);
#endif
        RenderPath2D::Render();
    }

    void RenderPath3D::Compose(CommandList cmd) const
    {
        renderer::ImageParams params{};
        params.EnableFullscreen();

        GraphicsDevice* device = rhi::GetDevice();
        device->BeginEvent("Composition", cmd);
        const rhi::ISampler* pointSampler = GetSamplerState(renderer::SSLOT_POINT_CLAMP);
        device->BindSampler(pointSampler, 0, cmd);
        renderer::DrawImage(rtMain, params, cmd);
        device->EndEvent(cmd);

        RenderPath2D::Compose(cmd);
    }
}
