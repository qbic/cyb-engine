#include "editor/widgets.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"

namespace cyb::ui
{
    void ItemLabel(const std::string& title, bool isLeft)
    {
        ImGuiWindow& window = *ImGui::GetCurrentWindow();
        const ImGuiStyle& style = ImGui::GetStyle();

        const ImVec2 lineStart = ImGui::GetCursorScreenPos();
        const float fullWidth = ImGui::GetContentRegionAvail().x;
        const float itemWidth = ImGui::CalcItemWidth() + style.ItemSpacing.x;
        const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());

        ImRect textRect;
        textRect.Min = ImGui::GetCursorScreenPos();
        if (!isLeft)
            textRect.Min.x = textRect.Min.x + itemWidth;
        textRect.Max = textRect.Min;
        textRect.Max.x += fullWidth - itemWidth;
        textRect.Max.y += textSize.y;

        ImGui::SetCursorScreenPos(textRect.Min);

        ImGui::AlignTextToFramePadding();
        // Adjust text rect manually because we render it directly into a drawlist instead of using public functions.
        textRect.Min.y += window.DC.CurrLineTextBaseOffset;
        textRect.Max.y += window.DC.CurrLineTextBaseOffset;

        ImGui::ItemSize(textRect);
        if (ImGui::ItemAdd(textRect, window.GetID(title.data(), title.data() + title.size())))
        {
            //const ColorScopeGuard guardColor{ ImGuiCol_Text, color.value_or(Color::BLACK).ToUInt(), color.has_value() };

            ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textRect.Min, textRect.Max, textRect.Max.x,
                textRect.Max.x, title.data(), title.data() + title.size(), &textSize);

            if (textRect.GetWidth() < textSize.x && ImGui::IsItemHovered())
                ImGui::SetTooltip("%.*s", (int)title.size(), title.data());
        }
        if (isLeft)
        {
            ImGui::SetCursorScreenPos(textRect.Max - ImVec2{ 0, textSize.y + window.DC.CurrLineTextBaseOffset });
            ImGui::SameLine();
        }
        else if (!isLeft)
            ImGui::SetCursorScreenPos(lineStart);
    }
}