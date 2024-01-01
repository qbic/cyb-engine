#include "hli/render-path-2d.h"
#include "editor/editor.h"
#include "imgui.h"
#include "editor/imgui-backend.h"
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
        //ImGui_Impl_CybEngine_Init();
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
            CYB_TRACE("Resizing buffers (width={} height={})", internalResolution.x, internalResolution.y);
            ResizeBuffers();
        }

#ifndef NO_EDITOR
        if (input::WasPressed(input::KEYBOARD_BUTTON_F1))
            showEditor = !showEditor;

        {
            CYB_PROFILE_CPU_SCOPE("GUI Update");
            editor::UpdateFPSCounter(dt);
            ImGui_Impl_CybEngine_Update();
            if (showEditor)
                editor::Update();
        }
#endif
    }

    void RenderPath2D::PostUpdate()
    {
    }

    void RenderPath2D::Render() const
    {
    }

    void RenderPath2D::Compose(graphics::CommandList cmd) const
    {
#ifndef NO_EDITOR
        {
            CYB_PROFILE_CPU_SCOPE("GUI Render");
            CYB_PROFILE_GPU_SCOPE("GUI", cmd);
            graphics::GetDevice()->BeginEvent("GUI", cmd);
            ImGui_Impl_CybEngine_Compose(cmd);
            graphics::GetDevice()->EndEvent(cmd);
        }
#endif
    }
}