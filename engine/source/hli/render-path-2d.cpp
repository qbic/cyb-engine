#include "hli/render-path-2d.h"
#include "editor/editor.h"
#include "imgui.h"
#include "editor/imgui-backend.h"
#include "ImGuizmo.h"

namespace cyb::hli
{
    void RenderPath2D::ResizeBuffers()
    {
    }

    void RenderPath2D::Load()
    {
#ifndef NO_EDITOR
        editor::Initialize();
        ImGui_Impl_CybEngine_Init();
#endif
    }

    void RenderPath2D::PreUpdate()
    {
    }

    void RenderPath2D::Update(float dt)
    {
#ifndef NO_EDITOR
        if (input::WasPressed(input::key::KB_F1))
            show_editor = !show_editor;

        ImGui_Impl_CybEngine_Update();
        editor::PushFrameTime(dt);
        if (show_editor)
            editor::Update();
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
        renderer::GetDevice()->BeginEvent("GUI", cmd);
        //editor::Render(cmd);
        ImGui_Impl_CybEngine_Compose(cmd);
        renderer::GetDevice()->EndEvent(cmd);
#endif
    }
}