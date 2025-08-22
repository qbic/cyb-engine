#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <variant>
#include "imgui.h"

namespace cyb::ui
{
    //------------------------------------------------------------------------------
    // Style / Color / ID helpers
    //------------------------------------------------------------------------------

    class StyleVarSet
    {
    public:
        using VarValue = std::variant<float, ImVec2>;
        StyleVarSet() = default;
        StyleVarSet(std::initializer_list<std::pair<ImGuiStyleVar, VarValue>> list);
        [[nodiscard]] size_t GetSize() const;
        void PushStyleVars() const;
        void PopStyleVars() const;

    private:
        std::unordered_map<ImGuiStyleVar, VarValue> m_values;
    };

    class StyleColorSet
    {
    public:
        using ColorValue = std::variant<ImVec4, ImU32, int>;
        StyleColorSet() = default;
        StyleColorSet(std::initializer_list<std::pair<ImGuiCol, ColorValue>> list);
        [[nodiscard]] size_t GetSize() const;
        void PushStyleColors() const;
        void PopStyleColors() const;

    private:
        std::unordered_map<ImGuiCol, ColorValue> m_values;
    };

    // Set a stylevar or stylevarscheme for imgui that will be reset when out of scope
    class ScopedStyleVar : private NonCopyable
    {
    public:
        using VarValue = StyleVarSet::VarValue;
        ScopedStyleVar(const StyleVarSet& varSet);
        ~ScopedStyleVar();

    private:
        uint32_t m_varCount;
    };

    // Set a stylecolor or stylecolorscheme for imgui that will be reset when out of scope
    class ScopedStyleColor : private NonCopyable
    {
    public:
        using ColorValue = StyleColorSet::ColorValue;
        ScopedStyleColor(const StyleColorSet& colorSet);
        ~ScopedStyleColor();

    private:
        uint32_t m_colorCount;
    };

    // Push an imgui id that will be popped when of scope
    class PushID : private NonCopyable
    {
    public:
        PushID(const void* id)      { ImGui::PushID(id); }
        PushID(const char* id)      { ImGui::PushID(id); }
        PushID(int id)              { ImGui::PushID(id); }
        ~PushID()                   { ImGui::PopID(); }
    };

    //------------------------------------------------------------------------------

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

    void SolidRect(ImU32 color, const ImVec2& size, const ImVec2& offset = ImVec2(0, 0), bool border = false);

    //------------------------------------------------------------------------------
    // Plotting functions, supporting mutiple plot lines on a single frame
    //------------------------------------------------------------------------------

    struct PlotLineDesc
    {
        std::string_view Label;
        ImU32 Color = 0;
        std::span<const float> Values = {};
    };

    void PlotMultiLines(
        const char* label,
        std::span<const PlotLineDesc> lines,
        const char* overlay_text = NULL,
        float scale_min = FLT_MAX,
        float scale_max = FLT_MAX,
        ImVec2 graph_size = ImVec2(0, 0));
}