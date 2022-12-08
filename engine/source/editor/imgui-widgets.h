//
// Gradient editor based on:
// https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112
//
#pragma once
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"
#include <string>
#include <unordered_map>
#include <fmt/format.h>

#define CYB_GUI_COMPONENT(func, label, ...) (ImGui::TableNextColumn(), ImGui::Text(label), ImGui::TableNextColumn(), ImGui::SetNextItemWidth(-FLT_MIN), func("##" label, __VA_ARGS__))

namespace cyb::editor::gui
{
    template <typename T>
    inline bool ComboBox(
        const std::string& label,
        T& value,
        const std::unordered_map<T, std::string>& combo)
    {
        bool change = false;
        const auto& selected = combo.find(value);
        const std::string& name = selected != combo.end() ? selected->second : "";

        if (ImGui::BeginCombo(label.c_str(), name.c_str()))
        {
            for (const auto& x : combo)
            {
                bool isSelected = (x.first == value);
                if (ImGui::Selectable(x.second.c_str(), isSelected))
                {
                    value = x.first;
                    change = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        return change;
    }
}

struct ImGradientMark
{
    ImColor color;
    float position;     // [0..1]
};

enum { kGradientBarEditorHeight = 40 };
enum { kGradientMarkDeleteDiffy = 40 };

struct ImGradient
{
    std::list<ImGradientMark*> markList;
    ImGradientMark* draggingMark = nullptr;
    ImGradientMark* selectedMark = nullptr;

    ImGradient()
    {
        AddMark(0.0f, ImColor(0.0f, 0.0f, 0.0f));
        AddMark(1.0f, ImColor(1.0f, 1.0f, 1.0f));
    }
    ImGradient(std::initializer_list<ImGradientMark> il)
    {
        for (const auto& x : il)
            AddMark(x.position, x.color);
    }

    ~ImGradient()
    {
        for (ImGradientMark* mark : markList)
            delete mark;
    }

    ImColor GetColorAt(float position) const
    {
        position = ImClamp(position, 0.0f, 1.0f);

        ImGradientMark* lower = nullptr;
        for (ImGradientMark* mark : markList)
        {
            if (mark->position <= position)
            {
                if (!lower || lower->position < mark->position)
                {
                    lower = mark;
                }   
            }

            if (mark->position > position)
                break;
        }

        if (lower == nullptr)
            return ImColor(0);

        return lower->color;
    }

    ImGradientMark*& AddMark(float position, ImColor const color)
    {
        position = ImClamp(position, 0.0f, 1.0f);
        ImGradientMark* newMark = new ImGradientMark();
        newMark->position = position;
        newMark->color = color;
        markList.push_back(newMark);
        ImGradientMark*& ret = markList.back();
        SortMarks();

        return ret;
    }

    void RemoveMark(ImGradientMark* mark)
    {
        markList.remove(mark);
    }

    void Clear()
    {
        markList.clear();
        draggingMark = nullptr;
        selectedMark = nullptr;
    }

    void SortMarks()
    {
        markList.sort([](const ImGradientMark* a, const ImGradientMark* b)
            {
                return a->position < b->position;
            });
    }

    ImGradient& operator=(const ImGradient& a)
    {
        Clear();
        for (const auto& mark : a.markList)
            AddMark(mark->position, mark->color);

        return *this;
    }
};

namespace ImGui
{
    inline bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
    {
        static auto itemGetter = [](void* vec, int idx, const char** outText)
        {
            auto& vector = *static_cast<std::vector<std::string>*>(vec);
            if (idx < 0 || idx >= static_cast<int>(vector.size()))
                return false;

            *outText = vector.at(idx).data();
            return true;
        };

        if (values.empty())
            return false;

        return Combo(label, currIndex, itemGetter, static_cast<void*>(&values), (int)values.size());
    }

    inline bool ListBox(const char* label, int* currIndex, std::vector<std::string_view>& values)
    {
        static auto itemGetter = [](void* vec, int idx, const char** outText)
        {
            auto& vector = *static_cast<std::vector<std::string_view>*>(vec);
            if (idx < 0 || idx >= static_cast<int>(vector.size()))
                return false;

            *outText = vector.at(idx).data();
            return true;
        };

        if (values.empty())
            return false;

        return ListBox(label, currIndex, itemGetter, static_cast<void*>(&values), (int)values.size());
    }

    /**
     * Draw a frame with the label inside and a filled bar with size 
     * Lerp(vMin, vMax, (v - vMin) / (vMax - vMin)) as background.
     */
    inline void FilledBar(const char* label, float v, float vMin, float vMax, const char* format = "{:.3f}")
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        const ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float width = CalcItemWidth();

        const ImVec2 labelSize = CalcTextSize(label, NULL, true);
        const ImRect frameBox(window->DC.CursorPos, window->DC.CursorPos + ImVec2(width, labelSize.y + style.FramePadding.y * 2.0f));
        const ImRect totalBox(frameBox.Min, frameBox.Max);
        ItemSize(totalBox, style.FramePadding.y);
        if (!ItemAdd(totalBox, id))
            return;

        std::string text = std::string(label) + ": "  + fmt::format(format, v);

        // Render
        RenderFrame(frameBox.Min, frameBox.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        const float fraction = (v - vMin) / (vMax - vMin);
        RenderRectFilledRangeH(window->DrawList, frameBox, GetColorU32(ImGuiCol_PlotHistogram), 0.0f, fraction, 0.0f);
        RenderText(ImVec2(frameBox.Min.x + style.ItemInnerSpacing.x, frameBox.Min.y + style.FramePadding.y), text.c_str());
    }

    static bool DrawGradientBar(ImGradient* gradient,
        const ImVec2& barPos,
        float maxWidth,
        float height)
    {
        bool modified = false;
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        const float barBottom = barPos.y + height;
        DrawList->AddRectFilled(ImVec2(barPos.x - 2, barPos.y - 2),
            ImVec2(barPos.x + maxWidth + 2, barBottom + 2),
            IM_COL32(100, 100, 100, 255));

        if (gradient->markList.size() == 0)
        {
            DrawList->AddRectFilled(ImVec2(barPos.x, barPos.y),
                ImVec2(barPos.x + maxWidth, barBottom),
                IM_COL32(255, 255, 255, 255));
        }

        ImColor color = ImColor(1, 1, 1);
        float prevX = barPos.x;
        const ImGradientMark* prevMark = nullptr;

        for (const auto& mark : gradient->markList)
        {
            assert(mark);
            const float from = prevX;
            const float to = prevX = barPos.x + mark->position * maxWidth;
            color = (prevMark == nullptr) ? mark->color : color = prevMark->color;

            if (mark->position > 0.0)
            {
                DrawList->AddRectFilled(
                    ImVec2(from, barPos.y),
                    ImVec2(to, barBottom),
                    color);
            }

            prevMark = mark;
        }

        if (prevMark && prevMark->position < 1.0)
        {
            DrawList->AddRectFilled(
                ImVec2(prevX, barPos.y),
                ImVec2(barPos.x + maxWidth, barBottom),
                gradient->markList.back()->color);
        }

        // Drag and dropping gradient to copy:
        const char* dragAndDropID = "_DragGradient";
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(dragAndDropID, &gradient, sizeof(ImGradient **));
            ImGui::Text("Move to another gradient to copy");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dragAndDropID);
            if (payload)
            {
                assert(payload->DataSize == sizeof(ImGradient *));
                ImGradient *draggedGradient = *((ImGradient**)payload->Data);
                *gradient = *draggedGradient;
                modified |= true;
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::SetCursorScreenPos(ImVec2(barPos.x, barPos.y + height + 10.0f));
        return modified;
    }

    static bool DrawGradientMarks(ImGradient* gradient,
        ImVec2 const& bar_pos,
        float maxWidth,
        float height)
    {
        bool modified = false;  // for color drag and drop
        ImGuiContext& g = *GImGui;
        float barBottom = bar_pos.y + height;
        ImGradientMark* prevMark = nullptr;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGradientMark*& draggingMark = gradient->draggingMark;
        ImGradientMark*& selectedMark = gradient->selectedMark;

        for (const auto& mark : gradient->markList)
        {
            assert(mark);
            if (!selectedMark)
            {
                selectedMark = mark;
                g.ColorPickerRef = selectedMark->color;
            }

            float to = bar_pos.x + mark->position * maxWidth;

            drawList->AddTriangleFilled(ImVec2(to, bar_pos.y + (height - 6)),
                ImVec2(to - 6, barBottom),
                ImVec2(to + 6, barBottom), IM_COL32(100, 100, 100, 255));

            drawList->AddRectFilled(ImVec2(to - 6, barBottom),
                ImVec2(to + 6, bar_pos.y + (height + 12)),
                IM_COL32(100, 100, 100, 255), 1.0f);

            drawList->AddRectFilled(ImVec2(to - 5, bar_pos.y + (height + 1)),
                ImVec2(to + 5, bar_pos.y + (height + 11)),
                IM_COL32(0, 0, 0, 255), 1.0f);

            if (selectedMark == mark)
            {
                const ImU32 frameColor = GetColorU32(ImGuiCol_Text);
                drawList->AddTriangleFilled(ImVec2(to, bar_pos.y + (height - 3)),
                    ImVec2(to - 4, barBottom + 1),
                    ImVec2(to + 4, barBottom + 1), frameColor);

                drawList->AddRect(ImVec2(to - 5, bar_pos.y + (height + 1)),
                    ImVec2(to + 5, bar_pos.y + (height + 11)),
                    frameColor, 1.0f);
            }

            drawList->AddRectFilled(ImVec2(to - 3, bar_pos.y + (height + 3)),
                ImVec2(to + 3, bar_pos.y + (height + 9)),
                mark->color);


            ImGui::SetCursorScreenPos(ImVec2(to - 6, barBottom));
            ImGui::InvisibleButton("mark", ImVec2(12, 12));

            if (BeginDragDropTarget())
            {
                ImVec4 col = {};
                const ImGuiPayload* payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F);
                if (payload)
                {
                    memcpy(&col.x, payload->Data, sizeof(float) * 4);
                    mark->color = ColorConvertFloat4ToU32(col);
                    modified |= true;
                }

                EndDragDropTarget();
            }
            
            const bool isHovered = IsItemHovered();
            if (isHovered)
            {
                if (ImGui::IsMouseClicked(0))
                {
                    selectedMark = mark;
                    draggingMark = mark;
                    g.ColorPickerRef = selectedMark->color;
                }
            }

            const bool isDraggingMark = (ImGui::IsMouseDragging(0) && (draggingMark == mark));
            if (isHovered || isDraggingMark)
            {
                BeginTooltip();
                Text("pos: %f", mark->position);
                EndTooltip();
            }

            prevMark = mark;
        }

        ImGui::SetCursorScreenPos(ImVec2(bar_pos.x, bar_pos.y + height + 20.0f));
        return modified;
    }

    inline bool GradientEditor(ImGradient* gradient)
    {
        bool modified = false;

        ImGuiContext& g = *GImGui;
        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        bar_pos.x += 10;
        float maxWidth = ImGui::GetContentRegionAvail().x - 20;
        float barBottom = bar_pos.y + kGradientBarEditorHeight;
        ImGradientMark*& draggingMark = gradient->draggingMark;
        ImGradientMark*& selectedMark = gradient->selectedMark;

        ImGui::InvisibleButton("gradient_editor_bar", ImVec2(maxWidth, kGradientBarEditorHeight));
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("Left-Click to add new mark");
            ImGui::EndTooltip();
        }

        // Create new mark on left mouse click
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            float pos = (ImGui::GetIO().MousePos.x - bar_pos.x) / maxWidth;
            ImColor newMarkColor = gradient->GetColorAt(pos);

            gradient->selectedMark = gradient->AddMark(pos, newMarkColor);
            g.ColorPickerRef = selectedMark->color;
        }

        modified |= DrawGradientBar(gradient, bar_pos, maxWidth, kGradientBarEditorHeight);
        modified |= DrawGradientMarks(gradient, bar_pos, maxWidth, kGradientBarEditorHeight);

        if (!ImGui::IsMouseDown(0) && gradient->draggingMark)
            gradient->draggingMark = nullptr;

        if (ImGui::IsMouseDragging(0) && gradient->draggingMark)
        {
            float increment = ImGui::GetIO().MouseDelta.x / maxWidth;
            bool insideZone = (ImGui::GetIO().MousePos.x > bar_pos.x) &&
                (ImGui::GetIO().MousePos.x < bar_pos.x + maxWidth);

            if (increment != 0.0f && insideZone)
            {
                draggingMark->position += increment;
                draggingMark->position = ImClamp(draggingMark->position, 0.0f, 1.0f);
                gradient->SortMarks();
                modified |= true;
            }

            float diffY = ImGui::GetIO().MousePos.y - barBottom;

            if (diffY >= kGradientMarkDeleteDiffy)
            {
                gradient->RemoveMark(draggingMark);
                draggingMark = nullptr;
                selectedMark = nullptr;
                modified |= true;
            }
        }

        if (!selectedMark && gradient->markList.size() > 0)
            selectedMark = gradient->markList.front();

        if (selectedMark)
            modified |= ImGui::ColorPicker4("color", &selectedMark->color.Value.x, ImGuiColorEditFlags_NoAlpha, &g.ColorPickerRef.x);

        return modified;
    }
    
    inline bool GradientButton(const char* label, ImGradient* gradient)
    {
        bool modified = false;
        assert(gradient);

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = CalcItemWidth();
        PushID(id);

        const ImVec2 labelSize = CalcTextSize(label, NULL, true);
        const ImRect frameBB(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, labelSize.y + style.FramePadding.y * 2.0f));
        const ImRect totalBB(frameBB.Min, frameBB.Max + ImVec2(labelSize.x > 0.0f ? style.ItemInnerSpacing.x + labelSize.x : 0.0f, 0.0f));
        ItemSize(totalBB, style.FramePadding.y);
        if (!ItemAdd(totalBB, id))
            return false;

        const float frameHeight = frameBB.Max.y - frameBB.Min.y;
        bool pressed = ButtonBehavior(frameBB, id, nullptr, nullptr);
        modified |= DrawGradientBar(gradient, frameBB.Min, frameBB.GetWidth(), frameHeight);

        if (pressed)
            OpenPopup("grad_edit");

        if (BeginPopup("grad_edit"))
        {
            modified |= GradientEditor(gradient);
            EndPopup();
        }        

        if (labelSize.x > 0.0f)
            RenderText(ImVec2(frameBB.Max.x + style.ItemInnerSpacing.x, frameBB.Min.y + style.FramePadding.y), label);

        PopID();
        return modified;
    }
}
