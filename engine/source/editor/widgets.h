#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <fmt/format.h>
#include "imgui/imgui.h"
#include "editor/undo_manager.h"

namespace cyb::ui {
    // Set a color or colorscheme for imgui that will be reset when out of scope
    class ScopedStyleColor final {
    public:
        ScopedStyleColor(ImGuiCol id, const ImVec4& color);
        ScopedStyleColor(ImGuiCol id, ImColor color);
        explicit ScopedStyleColor(const std::initializer_list<std::pair<ImGuiCol, ImVec4>> colors);
        explicit ScopedStyleColor(const std::initializer_list<std::pair<ImGuiCol, ImU32>> colors);
        ~ScopedStyleColor();

    private:
        uint32_t numColors;
    };

    // Push an imgui id that will be popped when of scope
    class ScopedID {
    public:
        template <class T>
        ScopedID(const T& label) {
            ImGui::PushID(label);
        }

        ScopedID(const std::string& label) {
            ImGui::PushID(label.c_str());
        }

        ~ScopedID() {
            ImGui::PopID();
        }
    };

    // Draw an info icon with mouse over text
    void InfoIcon(const char* fmt, ...);

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

    template <typename T,
        typename std::enable_if<std::is_enum<T>{} ||
        std::is_integral<std::underlying_type_t<T>>{}, bool>::type = true>
    void ComboBox(const char* label, T& value, const std::unordered_map<T, std::string>& combo, const std::function<void()>& onChange = nullptr) {
        ComboBox(label, (uint32_t&)value, reinterpret_cast<const std::unordered_map<uint32_t, std::string>&>(combo), onChange);
    }

    bool ListBox(const char* label, int* selectedIndex, std::vector<std::string_view>& values);

    struct GradientMark {
        ImColor color;
        float position = 0.0f;     // [0..1] range
    };

    struct Gradient {
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