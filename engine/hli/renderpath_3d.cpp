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
        const Viewport viewport = {
            .width = float(rtMain->GetDesc().width),
            .height = float(rtMain->GetDesc().height)
        };
        const Rect scissor = GetScissorInternalResolution();

        const std::array renderPassImages = std::to_array<RenderPassImage>({
            RenderPassImage::RenderTarget(
                rtMain,
                RenderPassImage::LoadOp::DontCare),
            RenderPassImage::DepthStencil(
                rtMainDepth,
                RenderPassImage::LoadOp::Clear,
                RenderPassImage::StoreOp::Store)
        });

        auto cmd = device->BeginCommandList();
        renderer::BindCameraCB(camera, cmd);
        renderer::UpdateRenderData(sceneViewMain, frameCB, cmd);

        cmd->BeginMarker("Opaque Scene");
        cmd->BindViewports(&viewport, 1);
        cmd->BeginRenderPass(renderPassImages.data(), renderPassImages.size());
        cmd->BindScissorRects(&scissor, 1);

        {
            CYB_PROFILE_GPU_SCOPE("Opaque Scene", cmd);
            renderer::DrawScene(sceneViewMain, cmd);
            renderer::DrawSky(sceneViewMain.camera, cmd);
        }

        {
            CYB_PROFILE_GPU_SCOPE("Debug Scene", cmd);
            renderer::DrawDebugScene(sceneViewMain, cmd);
        }

        cmd->EndMarker();
        cmd->EndRenderPass();

#if 1
        cmd->BeginMarker("Selection Outline");
        {
            CYB_PROFILE_GPU_SCOPE("Selection Outline", cmd);

            // Stencil fill pass
            const std::array rpStencilFill = std::to_array<RenderPassImage>({
                RenderPassImage::RenderTarget(
                    rtSelectionOutline,
                    RenderPassImage::LoadOp::Clear),
                RenderPassImage::DepthStencil(
                    rtMainDepth,
                    RenderPassImage::LoadOp::Load)
            });

            renderer::ImageParams image{
                .flags = renderer::ImageFlags::FullscreenBit,
                .stencilRef = 8,
                .stencilComp = renderer::STENCILMODE_EQUAL
            };

            cmd->BeginRenderPass(rpStencilFill.data(), rpStencilFill.size());
            cmd->BindSampler(GetSamplerState(renderer::SSLOT_POINT_CLAMP), 0);
            renderer::DrawImage(nullptr, image, cmd);
            cmd->EndRenderPass();

            // Stencil outline pass
            const std::array rpOutline = std::to_array<RenderPassImage>({
                RenderPassImage::RenderTarget(
                    rtMain,
                    RenderPassImage::LoadOp::Load)
            });

            cmd->BeginRenderPass(rpOutline.data(), rpOutline.size());
            XMFLOAT4 outlineColor = XMFLOAT4(1.0f, 0.62f, 0.17f, 1.0f);
            renderer::Postprocess_Outline(rtSelectionOutline, cmd, r_selectionOutlineThickness.GetValue(), 0.05f, outlineColor);
            cmd->EndRenderPass();
        }
        cmd->EndMarker();
#endif
        RenderPath2D::Render();
    }

    void RenderPath3D::Compose(ICommandList* cmd) const
    {
        renderer::ImageParams params{ .flags = renderer::ImageFlags::FullscreenBit };

        GraphicsDevice* device = rhi::GetDevice();
        const rhi::ISampler* pointSampler = GetSamplerState(renderer::SSLOT_POINT_CLAMP);

        cmd->BeginMarker("Composition");
        cmd->BindSampler(pointSampler, 0);
        renderer::DrawImage(rtMain, params, cmd);
        cmd->EndMarker();

        RenderPath2D::Compose(cmd);
    }
}
