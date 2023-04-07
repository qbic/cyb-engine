#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <optional>
#include <fmt/format.h>
#include "imgui/imgui.h"
#include "editor/icons_font_awesome6.h"
#include "editor/undo-manager.h"

namespace cyb::ui
{
    class ColorScopeGuard
    {
    public:
        ColorScopeGuard(ImGuiCol id, const ImVec4& color);
        ColorScopeGuard(ImGuiCol id, ImColor color);
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

    // Helper class for ImGui wrapper functions
    class WidgetScopedLayout
    {
    public:
        WidgetScopedLayout(const char* label);
        ~WidgetScopedLayout();

    private:
        IdScopeGuard m_idGuard;
    };

    template <class T>
    void SaveChangeToUndoManager(typename T::value_type* v)
    {
        if (ImGui::IsItemActivated())
        {
            auto action = std::make_shared<T>(v);
            ui::GetUndoManager().PushAction(action);
        }

        if (ImGui::IsItemDeactivatedAfterEdit())
            ui::GetUndoManager().CommitIncompleteAction();
    }

    // These are just ImGui wrappers wich will do the following:
    //  * Put the label to the left
    //  * Save changes to the undo manager
    bool Checkbox(const char* label, bool* v);
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);

    bool EditTransformComponent(const char* label, float component[3], scene::TransformComponent* transform);

    template <typename T>
    bool CheckboxFlags(const char* label, T* flags, T flags_value)
    {
        WidgetScopedLayout widget(label);
        bool change = ImGui::CheckboxFlags("", (unsigned int*)flags, (unsigned int)flags_value);
        SaveChangeToUndoManager<ui::EditorAction_ModifyValue<T, 1>>(flags);
        return change;
    }

    template <typename T>
    bool ComboBox(const char* label, T& value, const std::unordered_map<T, std::string>& combo)
    {
        WidgetScopedLayout widget(label);
        bool valueChange = false;
        const auto& selected = combo.find(value);
        const std::string& name = selected != combo.end() ? selected->second : "";

        if (ImGui::BeginCombo("", name.c_str()))
        {
            for (const auto& [key, name] : combo)
            {
                const bool isSelected = (key == value);
                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    const T tempValue[1] = { key };
                    auto action = std::make_shared<ui::EditorAction_ModifyValue<T, 1>>(&value, tempValue);
                    ui::GetUndoManager().PushAction(action);

                    value = key;
                    valueChange = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        return valueChange;
    }

    bool ListBox(const char* label, int* selectedIndex, std::vector<std::string_view>& values);
    void FilledBar(const char* label, float v, float vMin, float vMax, const char* format = "{:.2f}");

    struct GradientMark
    {
        ImColor color;
        float position;     // [0..1] range
    };

    struct Gradient
    {
        std::list<GradientMark*> markList;
        GradientMark* draggingMark = nullptr;
        GradientMark* selectedMark = nullptr;

        Gradient();
        Gradient(std::initializer_list<GradientMark> il);
        ~Gradient();

        ImColor GetColorAt(float position) const;
        GradientMark*& AddMark(float position, ImColor const color);
        void RemoveMark(GradientMark* mark);
        void Clear();
        void SortMarks();
        Gradient& operator=(const Gradient& a);
    };

    bool GradientButton(const char* label, Gradient* gradient);
}