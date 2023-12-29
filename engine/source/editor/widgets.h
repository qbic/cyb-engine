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
    // Set a color or colorscheme for imgui that will be reset when out of scope
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

    // Manually push an id to imgui that will be reset when out of scope
    class IdScopeGuard
    {
    public:
        template <class T>
        IdScopeGuard(const T label)
        {
            ImGui::PushID(label);
        }

        IdScopeGuard(const std::string& label)
        {
            ImGui::PushID(label.c_str());
        }

        ~IdScopeGuard()
        {
            ImGui::PopID();
        }
    };

    // These are basicly just ImGui wrappers wich uses left aligned labels, 
    // and saves any change to the undo manager
    void Checkbox(const char* label, bool* v, const std::function<void()> onChange);
    void CheckboxFlags(const char* label, uint32_t* flags, uint32_t flagsValue, const std::function<void()> onChange);
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
    void SliderFloat(const char* label, float* v, const std::function<void()> onChange, float minValue, float maxValue, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    void SliderInt(const char* label, int* v, const std::function<void()> onChange, int minValue, int maxValue, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
    bool EditTransformComponent(const char* label, float component[3], scene::TransformComponent* transform);
    void ComboBox(const char* label, uint32_t& value, const std::unordered_map<uint32_t, std::string>& combo, const std::function<void()>& onChange = nullptr);

    template <typename T>
    void ComboBox(const char* label, T& value, const std::unordered_map<T, std::string>& combo, const std::function<void()>& onChange = nullptr)
    {
        // Copying the map and converting type is slow
        // but it will be good ehough for now
        std::unordered_map<uint32_t, std::string> mapCopy;
        for (const auto& it : combo)
            mapCopy[(uint32_t)it.first] = it.second;
        ComboBox(label, (uint32_t&)value, mapCopy, onChange);
    }

    bool ListBox(const char* label, int* selectedIndex, std::vector<std::string_view>& values);
    void FilledBar(const char* label, float v, float vMin, float vMax, const char* format = "{:.2f}");

    struct GradientMark
    {
        ImColor color;
        float position = 0.0f;     // [0..1] range
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