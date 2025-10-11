#include "hli/renderpath_2d.h"
#include "editor/editor.h"
#include "imgui.h"
#include "editor/imgui_backend.h"
#include "ImGuizmo.h"

namespace cyb::hli
{
    void RenderPath2D::ResizeBuffers()
    {
        currentBufferSize = GetInternalResolution();
    }

    void RenderPath2D::Load()
    {
#ifndef NO_EDITOR
        editor::Initialize();
#endif
    }

    void RenderPath2D::PreUpdate()
    {
    }

    void RenderPath2D::Update(double dt)
    {
        const XMUINT2 internalResolution = GetInternalResolution();
        if (currentBufferSize.x != internalResolution.x || currentBufferSize.y != internalResolution.y)
        {
            CYB_TRACE("Resizing buffers (width={}, height={})", internalResolution.x, internalResolution.y);
            ResizeBuffers();
        }

#ifndef NO_EDITOR
        if (input::KeyPressed(input::KEYBOARD_BUTTON_F1))
            showEditor = !showEditor;

        {
            CYB_PROFILE_CPU_SCOPE("GUI Render");
            ImGui_Impl_CybEngine_Update();
            editor::Update(showEditor, dt);
        }
#endif
    }

    void RenderPath2D::PostUpdate()
    {
    }

    void RenderPath2D::Render() const
    {
    }

    void RenderPath2D::Compose(rhi::CommandList cmd) const
    {
#ifndef NO_EDITOR
        {
            CYB_PROFILE_GPU_SCOPE("GUI", cmd);
            rhi::GetDevice()->BeginEvent("GUI", cmd);
            ImGui_Impl_CybEngine_Compose(cmd);
            rhi::GetDevice()->EndEvent(cmd);
        }
#endif
    }
}