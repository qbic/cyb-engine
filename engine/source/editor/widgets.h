#pragma once
#include <string>
#include <fmt/format.h>
#include "imgui/imgui.h"
#include "editor/icons_font_awesome6.h"

namespace cyb::ui
{
    class ColorScopeGuard
    {
    public:
        ColorScopeGuard(ImGuiCol id, const ImVec4& color);
        ColorScopeGuard(ImGuiCol id, ImU32 color);
        explicit ColorScopeGuard(const std::initializer_list<std::pair<ImGuiCol, ImVec4>> colors);
        explicit ColorScopeGuard(const std::initializer_list<std::pair<ImGuiCol, ImU32>> colors);
        ~ColorScopeGuard();

    private:
        uint32_t m_numColors;
    };

    class IdScopeGuard
    {
    public:
        template <class T>
        IdScopeGuard(const T label)
        {
            ImGui::PushID(label);
        }

        ~IdScopeGuard()
        {
            ImGui::PopID();
        }
    };

    void ItemLabel(const std::string& title, bool isLeft = true);
}