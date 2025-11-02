/*
 * Gradient editor based on:
 * David Gallardo's https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112
 */
#include <memory>
#include <unordered_set>
#include <cmath>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "editor/undo_manager.h"
#include "editor/widgets.h"
#include "editor/icons_font_awesome6.h"
#include "imgui_internal.h"

namespace cyb::ui 
{
    StyleVarSet::StyleVarSet(std::initializer_list<std::pair<ImGuiStyleVar, VarValue>> list)
    {
        for (const auto& [key, val] : list)
            m_values.emplace(key, val);
    }

    void StyleVarSet::PushStyleVars() const
    {
        for (const auto& [styleVar, val] : m_values)
        {
            std::visit([&] (const auto& v) {
                ImGui::PushStyleVar(styleVar, v);
            }, val);
        }
    }

    void StyleVarSet::PopStyleVars() const
    {
        assert(m_values.size() < std::numeric_limits<int>::max());
        ImGui::PopStyleVar(static_cast<int>(m_values.size()));
    }

    StyleColorSet::StyleColorSet(std::initializer_list<std::pair<ImGuiCol, ColorValue>> list)
    {
        for (const auto& [key, val] : list)
            m_values.emplace(key, val);
    }

    void StyleColorSet::PushStyleColors() const
    {
        for (const auto& [styleColor, val] : m_values)
        {
            std::visit([&] (const auto& v) {
                ImGui::PushStyleColor(styleColor, v);
            }, val);
        }
    }

    void StyleColorSet::PopStyleColors() const
    {
        assert(m_values.size() < std::numeric_limits<int>::max());
        ImGui::PopStyleColor(static_cast<int>(m_values.size()));
    }

    ScopedStyleVar::ScopedStyleVar(const StyleVarSet& varSet) :
        m_varSet(varSet)
    {
        m_varSet.PushStyleVars();
    }

    ScopedStyleVar::~ScopedStyleVar()
    {
        m_varSet.PopStyleVars();
    }

    ScopedStyleColor::ScopedStyleColor(const StyleColorSet& colorSet) :
        m_colorSet(colorSet)
    {
        m_colorSet.PushStyleColors();
    }

    ScopedStyleColor::~ScopedStyleColor()
    {
        m_colorSet.PopStyleColors();
    }

    // Draw a left-aligned item label and place cursor on same line.
    // Returns the available width for the widget item.
#if 0
    float ItemLabel(const char* label)
    {
        constexpr float label_width_ratio = 0.5f;
        const ImGuiStyle& style = ImGui::GetStyle();
        const float avail_w = ImGui::CalcItemWidth();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 label_sz = ImGui::CalcTextSize(label);
        const ImRect bb{ pos, pos + ImVec2(avail_w * label_width_ratio, label_sz.y + style.FramePadding.y * 2.0f) };

        ImGui::AlignTextToFramePadding();
        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, 0))
            return avail_w - bb.GetWidth();

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGui::RenderTextEllipsis(draw_list, bb.Min, bb.Max, bb.Max.x, bb.Max.x, label, nullptr, &label_sz);
        if (bb.GetWidth() < label_sz.x && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", label);
        ImGui::SameLine();
        return avail_w - bb.GetWidth();
    }
#else
    float ItemLabel(const char* label)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const ImGuiStyle& style = GImGui->Style;
        const ImGuiID id = window->GetID(label);
        const float width = ImGui::CalcItemWidth();
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        // Right-align the frame_bb within the content region
        const float right_edge = window->WorkRect.Max.x - style.WindowPadding.x;
        const ImVec2 frame_max(right_edge, window->DC.CursorPos.y + label_size.y + style.FramePadding.y * 2.0f);
        const ImVec2 frame_min(frame_max.x - width, window->DC.CursorPos.y);
        const ImRect frame_bb(frame_min, frame_max);

        // Label goes to the left of the frame
        const ImVec2 label_offset(style.ItemInnerSpacing.x, 0.0f);
        const ImRect label_bb{ window->DC.CursorPos, ImVec2{frame_bb.Min.x - label_offset.x, window->DC.CursorPos.y + label_size.y + style.FramePadding.y * 2.0f } };
        const ImRect total_bb(label_bb.Min, frame_bb.Max);

        ImGui::ItemSize(label_bb, 0);
        if (ImGui::ItemAdd(label_bb, id, &frame_bb))
        {
            ImDrawList* draw_list = window->DrawList;
            ImGui::RenderTextEllipsis(draw_list, label_bb.Min, label_bb.Max, label_bb.Max.x, label_bb.Max.x, label, nullptr, &label_size);

            // Display tooltip on hover is label was clipped
            if (label_bb.GetWidth() < label_size.x && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", label);
        }

        // Setup cursor for the item
        ImGui::SetCursorScreenPos(frame_bb.Min);
        ImGui::SameLine();
        return width;
    }
#endif

    void InfoIcon(const char* fmt, ...) {
        ImGui::Text(ICON_FA_CIRCLE_INFO);
        if (ImGui::IsItemHovered()) {
            va_list args;
            va_start(args, fmt);
            ImGui::SetTooltipV(fmt, args);
            va_end(args);
        }
    }

    // Record any change to value v to the undo-manager, if value is
    // changed directly or though undo-manager onChanger() will be called
    template <class T, typename... Args>
    void SaveChangeToUndoManager(typename T::value_type* value, [[maybe_unused]] Args&&... args) {
        if (ImGui::IsItemActivated()) {
            ui::GetUndoManager().EmplaceAction<T>(value, std::forward<Args>(args)...);
        }

        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) {
            ui::GetUndoManager().ClearIncompleteAction();
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ui::GetUndoManager().CommitIncompleteAction();
        }
    }

#define COMMON_WIDGET_CODE(label)           \
    PushID m_idGuard{ label };              \
    float avail_width = ItemLabel(label);   \
    ImGui::SetNextItemWidth(avail_width);       // -1 messes up NG_Node rendering

    bool Checkbox(const char* label, bool* v, const std::function<void()> onChange)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        const bool pressed = ImGui::Checkbox("", v);
        SaveChangeToUndoManager<ui::ModifyValue<bool, 1>>(v, onChange);
        return pressed;
    }

    void CheckboxFlags(const char* label, uint32_t* flags, uint32_t flagsValue, const std::function<void()> onChange)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        ImGui::CheckboxFlags("", (unsigned int*)flags, (unsigned int)flagsValue);
        SaveChangeToUndoManager<ui::ModifyValue<uint32_t, 1>>(flags, onChange);
    }

    bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        bool change = ImGui::DragFloat("", v, v_speed, v_min, v_max, format, flags);
        SaveChangeToUndoManager<ui::ModifyValue<float, 1>>(v);
        return change;
    }
    
    bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        bool change = ImGui::DragFloat3("", v, v_speed, v_min, v_max, format, flags);
        SaveChangeToUndoManager<ui::ModifyValue<float, 3>>(v);
        return change;
    }

    bool DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        bool change = ImGui::DragInt("", v, v_speed, v_min, v_max, format, flags);
        SaveChangeToUndoManager<ui::ModifyValue<int, 1>>(v);
        return change;
    }

    bool SliderFloat(const char* label, float* v, const std::function<void()> onChange, float minValue, float maxValue, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        float temp = *v;
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        const bool change = ImGui::SliderFloat("", &temp, minValue, maxValue);
        SaveChangeToUndoManager<ui::ModifyValue<float, 1>>(v, onChange);
        *v = temp;
        return change;
    }

    bool SliderInt(const char* label, int* v, const std::function<void()> onChange, int minValue, int maxValue, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        int temp = *v;
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        const bool change = ImGui::SliderInt("", &temp, minValue, maxValue, format, flags);
        SaveChangeToUndoManager<ui::ModifyValue<int, 1>>(v, onChange);
        *v = temp;
        return change;
    }

    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        bool change = ImGui::ColorEdit3("", col, flags);
        SaveChangeToUndoManager<ui::ModifyValue<float, 3>>(col);
        return change;
    }

    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        bool change = ImGui::ColorEdit4("", col, flags);
        SaveChangeToUndoManager<ui::ModifyValue<float, 4>>(col);
        return change;
    }

    bool EditTransformComponent(const char* label, float component[3], scene::TransformComponent* transform)
    {
        // Componenet must reside within TransformComponent!
        assert((uint8_t*)component >= (uint8_t*)transform && (uint8_t*)component < ((uint8_t*)transform + sizeof(*transform)));
        COMMON_WIDGET_CODE(label);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        bool change = ImGui::DragFloat3("", component, 0.1f, 0.0f, 0.0f, "%.1f");
        ImGui::PopStyleVar();

        if (change)
            transform->SetDirty();
        
        SaveChangeToUndoManager<ui::ModifyTransform>(transform);

        return change;
    }

    void ComboBox(const char* label, uint32_t& value, const std::unordered_map<uint32_t, std::string>& combo, const std::function<void()>& onChange)
    {
        COMMON_WIDGET_CODE(label);
        bool valueChange = false;
        const auto& selected = combo.find(value);
        const std::string& name = selected != combo.end() ? selected->second : "";

        ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
        if (ImGui::BeginCombo("", name.c_str()))
        {
            for (const auto& [key, name] : combo)
            {
                const bool isSelected = (key == value);
                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    const uint32_t tempValue[1] = { key };
                    ui::GetUndoManager().EmplaceAction<ui::ModifyValue<uint32_t, 1>>(&value, tempValue, onChange);

                    value = key;
                    valueChange = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (valueChange && onChange)
            onChange();
    }

    bool ListBox(const char* label, int* selectedIndex, std::vector<std::string_view>& values)
    {
        COMMON_WIDGET_CODE(label);
        auto getter = [](void* vec, int idx) -> const char *
        {
            auto& vector = *static_cast<std::vector<std::string_view>*>(vec);
            if (idx < 0 || idx >= static_cast<int>(vector.size()))
                return nullptr;

            return vector[idx].data();
        };

        bool change = false;
        if (!values.empty())
        {
            ScopedStyleVar styleVars({ { ImGuiStyleVar_FrameBorderSize, 1.0f } });
            change = ImGui::ListBox("", selectedIndex, getter, static_cast<void*>(&values), (int)values.size());
        }

        return change;
    }

#define GRADIENT_BAR_EDITOR_HEIGHT      40
#define GRADIENT_MARK_DELETE_DIFFY      40

    Gradient::Gradient()
    {
        AddMark(0.0f, ImColor(0.0f, 0.0f, 0.0f));
        AddMark(1.0f, ImColor(1.0f, 1.0f, 1.0f));
    }
        
    Gradient::Gradient(std::initializer_list<GradientMark> il)
    {
        for (const auto& x : il)
            AddMark(x.position, x.color);
    }

    Gradient::~Gradient()
    {
        for (GradientMark* mark : markList)
            delete mark;
    }

    ImColor Gradient::GetColorAt(float position) const
    {
        position = ImClamp(position, 0.0f, 1.0f);

        GradientMark* lower = nullptr;
        for (GradientMark* mark : markList)
        {
            assert(mark);
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

    GradientMark*& Gradient::AddMark(float position, ImColor const color)
    {
        position = ImClamp(position, 0.0f, 1.0f);
        GradientMark* newMark = new GradientMark();
        newMark->position = position;
        newMark->color = color;
        markList.push_back(newMark);
        GradientMark*& ret = markList.back();
        SortMarks();

        return ret;
    }

    void Gradient::RemoveMark(GradientMark* mark)
    {
        markList.remove(mark);
    }

    void Gradient::Clear()
    {
        markList.clear();
        draggingMark = nullptr;
        selectedMark = nullptr;
    }

    void Gradient::SortMarks()
    {
        markList.sort([](const GradientMark* a, const GradientMark* b) {
            return a->position < b->position;
        });
    }

    Gradient& Gradient::operator=(const Gradient& a)
    {
        Clear();
        for (const auto& mark : a.markList)
            AddMark(mark->position, mark->color);

        return *this;
    }

    static bool DrawGradientBar(Gradient* gradient, const ImVec2& barPos, float maxWidth, float height)
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
        const GradientMark* prevMark = nullptr;

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
            ImGui::SetDragDropPayload(dragAndDropID, &gradient, sizeof(Gradient**));
            ImGui::Text("Move to another gradient to copy");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dragAndDropID);
            if (payload)
            {
                assert(payload->DataSize == sizeof(Gradient*));
                Gradient* draggedGradient = *((Gradient**)payload->Data);
                *gradient = *draggedGradient;
                modified |= true;
            }

            ImGui::EndDragDropTarget();
        }

        return modified;
    }

    static bool DrawGradientMarks(Gradient* gradient,
        ImVec2 const& bar_pos,
        float maxWidth,
        float height)
    {
        bool modified = false;  // for color drag and drop
        ImGuiContext& g = *GImGui;
        float barBottom = bar_pos.y + height;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        GradientMark*& draggingMark = gradient->draggingMark;
        GradientMark*& selectedMark = gradient->selectedMark;

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
                const ImU32 frameColor = ImGui::GetColorU32(ImGuiCol_Text);
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

            if (ImGui::BeginDragDropTarget())
            {
                ImVec4 col = {};
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F);
                if (payload)
                {
                    memcpy(&col.x, payload->Data, sizeof(float) * 4);
                    mark->color = ImGui::ColorConvertFloat4ToU32(col);
                    modified |= true;
                }

                ImGui::EndDragDropTarget();
            }

            const bool isHovered = ImGui::IsItemHovered();
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
                ImGui::BeginTooltip();
                ImGui::Text("pos: %f", mark->position);
                ImGui::EndTooltip();
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(bar_pos.x, bar_pos.y + height + 20.0f));
        return modified;
    }

    static bool GradientEditor(Gradient* gradient)
    {
        bool modified = false;

        ImGuiContext& g = *GImGui;
        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        bar_pos.x += 10;
        float maxWidth = ImGui::GetContentRegionAvail().x - 20;
        float barBottom = bar_pos.y + GRADIENT_BAR_EDITOR_HEIGHT;
        GradientMark*& draggingMark = gradient->draggingMark;
        GradientMark*& selectedMark = gradient->selectedMark;

        ImGui::InvisibleButton("gradient_editor_bar", ImVec2(maxWidth, GRADIENT_BAR_EDITOR_HEIGHT));
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

        modified |= DrawGradientBar(gradient, bar_pos, maxWidth, GRADIENT_BAR_EDITOR_HEIGHT);
        modified |= DrawGradientMarks(gradient, bar_pos, maxWidth, GRADIENT_BAR_EDITOR_HEIGHT);

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

            if (diffY >= GRADIENT_MARK_DELETE_DIFFY)
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

    bool GradientButtonImpl(Gradient* gradient)
    {
        assert(gradient);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImGui::CalcItemSize(ImVec2(-1, ImGui::GetFrameHeight()), style.FramePadding.x * 2.0f, style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, ImGui::GetID(gradient)))
            return false;

        const float frameHeight = size.y - style.FramePadding.y;
        if (ImGui::ButtonBehavior(bb, ImGui::GetItemID(), nullptr, nullptr))
            ImGui::OpenPopup("grad_edit");
        bool modified = DrawGradientBar(gradient, bb.Min, bb.GetWidth(), frameHeight);

        if (ImGui::BeginPopup("grad_edit"))
        {
            modified |= GradientEditor(gradient);
            ImGui::EndPopup();
        }

        return modified;
    }

    bool GradientButton(const char* label, Gradient* gradient)
    {
        COMMON_WIDGET_CODE(label);
        return GradientButtonImpl(gradient);
    }

    using namespace ImGui;

    void SolidRect(ImU32 color, const ImVec2& size, const ImVec2& offset, bool border)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        const ImGuiStyle& style = g.Style;
        const ImVec2 frame_size = CalcItemSize(size, CalcItemWidth(), style.FramePadding.y * 2.0f);
        const ImRect frame_bb(window->DC.CursorPos + offset, window->DC.CursorPos + offset +frame_size);
        const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
        const ImRect total_bb(frame_bb.Min, frame_bb.Max);
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, 0, &frame_bb))
            return;

        RenderFrame(frame_bb.Min, frame_bb.Max, color, border, 0);
    }

    //------------------------------------------------------------------------------
    // Plotting functions
    //------------------------------------------------------------------------------

    void PlotMultiLines(
        const char* label,
        std::span<const PlotLineDesc> lines,
        const char* overlay_text,
        float scale_min,
        float scale_max,
        ImVec2 graph_size)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const ImVec2 frame_size = CalcItemSize(graph_size, CalcItemWidth(), label_size.y + style.FramePadding.y * 2.0f);

        const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
        const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
        const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, 0, &frame_bb, ImGuiItemFlags_NoNav))
            return;

        // Determine scale from values if not specified
        if (scale_min == FLT_MAX || scale_max == FLT_MAX)
        {
            float v_min = FLT_MAX;
            float v_max = -FLT_MAX;
            for (const auto& line : lines)
            {
                for (const float v : line.Values)
                {
                    if (v != v) // Ignore NaN values
                        continue;
                    v_min = ImMin(v_min, v);
                    v_max = ImMax(v_max, v);
                }
                if (scale_min == FLT_MAX)
                    scale_min = v_min;
                if (scale_max == FLT_MAX)
                    scale_max = v_max;
            }
        }

        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        const int values_count_min = 2;
        for (const auto& line : lines)
        {
            if (line.Values.size() >= values_count_min)
            {
                int res_w = ImMin((int)frame_size.x, (int)line.Values.size()) - 1;
                size_t item_count = line.Values.size() - 1;

                const float t_step = 1.0f / (float)res_w;
                const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

                float v0 = line.Values[0];
                float t0 = 0.0f;
                ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale));                       // Point in the normalized space of our target rectangle
                float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (1 + scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);   // Where does the zero line stands

                for (int n = 0; n < res_w; n++)
                {
                    const float t1 = t0 + t_step;
                    const int v1_idx = (int)(t0 * item_count + 0.5f);
                    IM_ASSERT(v1_idx >= 0 && v1_idx < line.Values.size());
                    const float v1 = line.Values[(v1_idx + 1) % line.Values.size()];
                    const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

                    // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
                    ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
                    ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, tp1);
                    window->DrawList->AddLine(pos0, pos1, line.Color);

                    t0 = t1;
                    tp0 = tp1;
                }
            }
        }

        // Labels
        float y_offset = 0.0f;
        for (const auto& line : lines)
        {
            ImVec2 pos = ImVec2(frame_bb.Min.x, frame_bb.Min.y + y_offset) + style.FramePadding;
            ImVec2 rect_size = ImVec2(12, 12);

            int HACK_RECT_ALIGN = 4;
            RenderFrame(ImVec2(pos.x, pos.y + HACK_RECT_ALIGN), ImVec2(pos.x + rect_size.x, pos.y + rect_size.y + HACK_RECT_ALIGN), line.Color, false);
            
            ImVec2 text_pos = ImVec2(pos.x + rect_size.x + style.FramePadding.x, pos.y );
            RenderTextClipped(text_pos, frame_bb.Max, line.Label.data(), NULL, NULL);

            y_offset += rect_size.y + style.FramePadding.y;
        }

        // Text overlay
        if (overlay_text)
            RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

        if (label_size.x > 0.0f)
            RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
    }

    //------------------------------------------------------------------------------
    // Node graph functions
    //------------------------------------------------------------------------------

#ifdef CYB_DEBUG_NG_RECTS
#define DRAW_DEBUG_RECT() ImGui::DebugDrawItemRect();
#else
#define DRAW_DEBUG_RECT()
#endif

    void NG_Node::PushWindowWorkRect(const NG_Canvas& canvas)
    {
        const NG_Style& style = canvas.Style;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float width = m_NodeMaxWidth * canvas.Zoom;

        m_ParentWorkRectMin = window->WorkRect.Min;
        m_ParentWorkRectMax = window->WorkRect.Max;

        window->WorkRect.Min.x = window->WorkRect.Min.x + Pos.x* canvas.Zoom + canvas.Offset.x + style.NodeWindowPadding.x * canvas.Zoom;
        window->WorkRect.Max.x = window->WorkRect.Min.x + width + style.NodeWindowPadding.x * canvas.Zoom;

#ifdef CYB_DEBUG_NG_RECTS
        window->DrawList->AddRect(window->WorkRect.Min, window->WorkRect.Max, 0xff00ffff);
#endif
        ImGui::PushItemWidth(width * 0.5f);
    }

    void NG_Node::PopWindowWorkRect()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::PopItemWidth();
        window->WorkRect.Min = m_ParentWorkRectMin;
        window->WorkRect.Max = m_ParentWorkRectMax;
    }

    void NG_Factory::DrawMenuContent(NG_Canvas& canvas, const ImVec2& popupPos)
    {
        for (auto& [type_str, createCallback] : m_availableNodeTypesMap)
        {
            if (ImGui::MenuItem(type_str.c_str()))
                canvas.Nodes.push_back(std::move(CreateNode(type_str, popupPos)));
        }
    }

    std::unique_ptr<NG_Node> NG_Factory::CreateNode(const std::string& node_type, const ImVec2& pos) const
    {
        auto it = m_availableNodeTypesMap.find(node_type);
        if (it != m_availableNodeTypesMap.end())
        {
            auto newNode = it->second();
            newNode->Pos = pos;
            return newNode;
        }

        assert(0);
        return nullptr;
    }

    NG_Canvas::NG_Canvas()
    {
        // Create a default factory
        Factory = std::make_unique<NG_Factory>();
    }

    bool NG_Canvas::IsPinConnected(detail::NG_Pin* pin) const
    {
        for (auto& connection : Connections)
            if (connection.From == pin || connection.To == pin)
                return true;
        return false;
    }

    bool NG_Canvas::WouldCreateCycle(const NG_Node* from, const NG_Node* to) const
    {
        std::unordered_set<const NG_Node*> visited;
        std::stack<const NG_Node*> stack;

        // DFS (depth-first-search) cycle detection
        stack.push(to);
        while (!stack.empty())
        {
            const NG_Node* current = stack.top();
            stack.pop();

            if (current == from)
                return true; // Cycle detected

            if (!visited.insert(current).second)
                continue;

            // Follow outputs from current node
            for (auto& connection : Connections)
            {
                if (connection.From->ParentNode == current)
                    stack.push(connection.To->ParentNode);
            }
        }

        return false;
    }

    const detail::NG_Connection* NG_Canvas::FindConnectionTo(detail::NG_Pin* pin) const
    {
        assert(pin);
        for (auto& connection : Connections)
        {
            if (connection.To == pin)
                return &connection;
        }

        return nullptr;
    }

    std::vector<const detail::NG_Connection*> NG_Canvas::FindConnectionsFrom(detail::NG_Pin* pin) const
    {
        assert(pin);
        std::vector<const detail::NG_Connection*> result;
        for (auto& c : Connections)
        {
            if (c.From == pin)
                result.push_back(&c);
        }
        return result;
    }

    void NG_Canvas::UpdateAllValidStates()
    {
        // TODO: Optimize (cache checked nodes)
        for (auto& node : Nodes)
            node->ValidState = NodeHasValidState(node.get());
    }


    bool NG_Canvas::NodeHasValidState(NG_Node* node) const
    {
        for (auto& pin : node->Inputs)
        {
            // Check input pin
            const detail::NG_Connection* connection = FindConnectionTo(pin.get());
            if (!connection)
                return false;

            // Check parent node
            NG_Node* parent = connection->From->ParentNode;
            if (!NodeHasValidState(parent))
                return false;
        }

        return true;
    }

    void NG_Canvas::SendUpdateSignalFrom(NG_Node* node)
    {
        assert(node);
        for (auto& pin : node->Outputs)
        {
            for (auto& connection : FindConnectionsFrom(pin.get()))
            {
                connection->To->ParentNode->Update();
                SendUpdateSignalFrom(connection->To->ParentNode);
            }
        }
    }

    bool NG_Canvas::UpdateHoveredNode()
    {
        HoveredNode = nullptr;
        for (auto it = Nodes.rbegin(); it != Nodes.rend(); ++it)
        {
            const NG_Node* node = it->get();
            const ImVec2 node_start = Pos + Offset + node->Pos * Zoom;
            if (ImGui::IsMouseHoveringRect(node_start, node_start + node->Size))
            {
                HoveredNode = node;
                break;
            }
        }

        return HoveredNode != nullptr;
    }

    bool NG_Canvas::IsNodeHovered(ImGuiID node_id) const
    {
        if (!HoveredNode)
            return false;
        return node_id == HoveredNode->GetID();
    }

    void NG_Canvas::BringNodeToDisplayFront(ImGuiID node_id)
    {
        // Cheap early out
        if (Nodes.size() == 0 || node_id == Nodes.back()->GetID())
            return;

        auto it = std::find_if(Nodes.begin(), Nodes.end(), [&] (auto& n) {
            return n->GetID() == node_id;
        });

        if (it != Nodes.end())
        {
            std::unique_ptr<NG_Node> tmp = std::move(*it);
            Nodes.erase(it);
            Nodes.push_back(std::move(tmp));
        }
    }

    static void DrawGrid(const ImVec2& pos, const ImVec2& size, const ImVec2& offset, float step, ImU32 color)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (float x = std::fmod(offset.x, step); x < size.x; x += step)
            draw_list->AddLine(ImVec2(pos.x + x, pos.y), ImVec2(pos.x + x, pos.y + size.y), color);
        for (float y = std::fmod(offset.y, step); y < size.y; y += step)
            draw_list->AddLine(ImVec2(pos.x, pos.y + y), ImVec2(pos.x + size.x, pos.y + y), color);
    }

    static void DrawNode(NG_Node& node, const NG_Canvas& canvas)
    {
        const NG_Style& style = canvas.Style;
        const ImVec2 frame_padding = style.NodeFramePadding * canvas.Zoom;
        const ImVec2 window_padding = style.NodeWindowPadding * canvas.Zoom;
        const ImVec2 space_size = ImGui::CalcTextSize(" ") * canvas.Zoom;
        const float pin_radius = style.PinRadius * canvas.Zoom;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        bool item_hovered = false;

        ImGui::PushID(node.GetID());
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !canvas.IsNodeHovered(node.GetID()));

        // 0 = Background, 1 = Foreground
        draw_list->ChannelsSplit(2);

        ImGui::BeginGroup();
        draw_list->ChannelsSetCurrent(1);

        // Calculate space for node label.
        const ImVec2 node_start = canvas.Pos + canvas.Offset + node.Pos * canvas.Zoom;
        const ImVec2 label_sz = ImGui::CalcTextSize(node.GetLabel().c_str()) + window_padding;
#ifdef CYB_DEBUG_NG_RECTS
        draw_list->AddRect(node_start, node_start + label_sz, 0xff0000ff);
#endif

        // Calculate space for pins, but draw them later when node width is known
        // NOTE: We do frame_padding.y * 4.0f for some extra padding between pins and body
        const ImVec2 pins_start = node_start + ImVec2(0, label_sz.y);
        const size_t pin_count = ImMax(node.Inputs.size(), node.Outputs.size());
        float pins_width = 0.0f;
        for (size_t i = 0; i < pin_count; ++i)
        {
            const char* inLabel = (i < node.Inputs.size()) ? node.Inputs[i]->Label.c_str() : "";
            const char* outLabel = (i < node.Outputs.size()) ? node.Outputs[i]->Label.c_str() : "";
            const float width = ImGui::CalcTextSize(inLabel).x + ImGui::CalcTextSize(outLabel).x;
            pins_width = ImMax(pins_width, width);
        }
        const ImVec2 pins_sz = ImVec2(pins_width + (space_size.x / canvas.Zoom) * 4.0f, (pin_count * ImGui::GetTextLineHeight()) + frame_padding.y * 4.0f);
#ifdef CYB_DEBUG_NG_RECTS
        draw_list->AddRect(pins_start, pins_start + pins_sz, 0xff00ffff);
#endif

        // Display node body
        const ImVec2 body_start = pins_start + ImVec2(0, pins_sz.y);
        ImGui::SetCursorScreenPos(body_start + ImVec2(window_padding.x, 0));
        ImGui::BeginGroup();
        node.PushWindowWorkRect(canvas);
        node.DisplayContent();
        node.PopWindowWorkRect();
#ifdef CYB_DEBUG_NG_CANVAS_STATE
        ImGui::Text("----------------");
        ImGui::Text("ID: %u", node.GetID());
        ImGui::Text("Hovered: %s", canvas.IsNodeHovered(node.GetID()) ? "true" : "false");
        ImGui::Text("Active: %s", ImGui::GetActiveID() == node.GetID() ? "true" : "false");
        ImGui::Text("Pos: [%.1f, %.1f]", node.Pos.x, node.Pos.y);
        ImGui::Text("Size: [%.1f, %.1f]", node.Size.x, node.Size.y);
#endif
        ImGui::EndGroup();
        const ImVec2 body_sz = ImGui::GetItemRectSize() + ImVec2(window_padding.x * 2, window_padding.y);
        const float node_width = ImMax(pins_sz.x, body_sz.x);
        DRAW_DEBUG_RECT();

        // Add a close button
        static constexpr float HEADER_BUTTON_SPACING = 6.0f;
        ImGui::GetStyle().HoverDelayNormal = 1.5f;

        const ImVec2 header_btn_sz = ImVec2( 14.0f, 14.0f ) * canvas.Zoom;
        ImRect close_btn_bb{ node_start + ImVec2(node_width - header_btn_sz.x - window_padding.x, window_padding.y),
                             node_start + ImVec2(node_width - window_padding.x, window_padding.y + header_btn_sz.y) };
        const ImGuiID close_btn_id = ImGui::GetID("#CLOSE");
        if (ImGui::ItemAdd(close_btn_bb, close_btn_id))
        {
            if (ImGui::ButtonBehavior(close_btn_bb, close_btn_id, nullptr, nullptr))
                node.MarkedForDeletion = true;
            DRAW_DEBUG_RECT();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("Close");
            item_hovered |= ImGui::IsItemHovered();
            const ImColor close_btn_col = ImGui::IsItemHovered() ? ImColor(255, 96, 88) : ImColor(255, 96, 88, 150);
            draw_list->AddCircleFilled(close_btn_bb.GetCenter(), close_btn_bb.GetWidth() * 0.5f, close_btn_col);
        }

        auto draw_pin = [&] (detail::NG_Pin* pin, bool leftAligned) -> bool {
            const ImVec2 text_sz = ImGui::CalcTextSize(pin->Label.c_str());
            const float x_pos = leftAligned ?
                node_start.x + window_padding.x + pin_radius :
                node_start.x + node_width - text_sz.x - pin_radius - window_padding.x;

            ImGui::SetCursorScreenPos(ImVec2(x_pos, ImGui::GetCursorScreenPos().y));
            ImGui::Text(pin->Label.c_str());
            const ImVec2 line_pos = ImGui::GetItemRectMin();

            const float y_pos = line_pos.y + text_sz.y * 0.5f;
            pin->Pos = leftAligned ?
                ImVec2(line_pos.x - window_padding.x - pin_radius, y_pos) :
                ImVec2(node_start.x + node_width, y_pos);
            const ImRect pin_bb{ pin->Pos - ImVec2(pin_radius, pin_radius),
                                 pin->Pos + ImVec2(pin_radius, pin_radius)};
            if (ImGui::ItemAdd(pin_bb, ImGui::GetID(&pin)))
            {
                DRAW_DEBUG_RECT();
                pin->Hovered = ImGui::ItemHoverable(pin_bb, ImGui::GetID(&pin), 0);
                const bool connected = canvas.IsPinConnected(pin);
                const ImColor pin_col = pin->Hovered ? style.PinHoverColor : connected ? style.PinConnectedColor : style.PinUnConnectedColor;
                draw_list->AddCircleFilled(pin->Pos, pin_radius, pin_col);
                draw_list->AddCircle(pin->Pos, pin_radius, style.NodeBorderColor, 0, 2.1f * canvas.Zoom);
            }
            return pin->Hovered;
        };

        // Display input pins
        ImGui::SetCursorScreenPos(pins_start + frame_padding + ImVec2(window_padding.x + pin_radius, 0));
        for (auto& pin : node.Inputs)
            item_hovered |= draw_pin(pin.get(), true);

        // Display output pins
        ImGui::SetCursorScreenPos(pins_start + frame_padding + ImVec2(window_padding.x, 0));
        for (auto& pin : node.Outputs)
            item_hovered |= draw_pin(pin.get(), false);

        // Display node label center aligned
        ImGui::SetCursorScreenPos(node_start + ImVec2(node_width * 0.5f - label_sz.x * 0.5f, 0));
        ImGui::Text(node.GetLabel().c_str());
        ImGui::EndGroup();

        // Display background & border
        draw_list->ChannelsSetCurrent(0);

        node.Size = ImVec2(node_width, label_sz.y + pins_sz.y + body_sz.y);
        const ImRect node_bb{ node_start, node_start + node.Size };
        draw_list->AddRectFilled(node_bb.Min, node_bb.Max, style.NodeBackgroundColor, style.NodeFrameRounding * canvas.Zoom);
        const ImColor borderColor = node.ValidState ? style.NodeBorderColor : style.NodeBorderInvalidColor;
        draw_list->AddRect(node_bb.Min, node_bb.Max, borderColor, style.NodeFrameRounding * canvas.Zoom, 0, 2.1f * canvas.Zoom);

        draw_list->ChannelsMerge();

        // Add an item to handle dragging
        ImGui::SetCursorScreenPos(node_start);
        ImGui::ItemAdd(node_bb, node.GetID(), nullptr);
        ImGui::ItemSize(node.Size);
        DRAW_DEBUG_RECT();

        const bool any_active = ImGui::IsAnyItemActive();
        if (canvas.IsNodeHovered(node.GetID()) && !any_active && !item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            ImGui::SetActiveID(node.GetID(), ImGui::GetCurrentWindow());

        if (ImGui::IsItemActive() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            ImGui::ClearActiveID();

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            node.Pos += ImGui::GetIO().MouseDelta / canvas.Zoom;

        ImGui::PopItemFlag();
        ImGui::PopID();
    }

    bool NodeGraph(NG_Canvas& canvas)
    {
        static constexpr float ZOOM_SPEED = 0.1f;
        static constexpr float ZOOM_SNAP_EPSILON = 0.05;
        static constexpr float MIN_ZOOM = 0.2f;
        static constexpr float MAX_ZOOM = 4.0f;
        static constexpr float GRID_SIZE = 50.0f;
        static constexpr float GRID_SUBDIVISIONS = 4.0f;
        static constexpr float SUBGRID_ZOOM_THRESHOLD = 1.2f;
        static constexpr ImU32 GRID_COLOR = IM_COL32(90, 90, 90, 255);
        static constexpr ImU32 GRID_SUBGRID_COLOR = IM_COL32(50, 50, 50, 255);

        ImGuiID canvas_id = ImGui::GetID(&canvas);
        ImGui::PushID(canvas_id);

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        NG_Style& style = canvas.Style;
        if (!ImGui::ItemAdd(window->ContentRegionRect, canvas_id))
        {
            ImGui::PopID();
            return false;
        }

        canvas.Pos = ImGui::GetCursorScreenPos();
        const ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

        // Handle panning with middle mouse button
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            canvas.Offset += io.MouseDelta;

        // Handle zoom with mouse wheel
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
        {
            const float prev_zoom = canvas.Zoom;
            const float log_zoom = std::logf(canvas.Zoom) + io.MouseWheel * ZOOM_SPEED;
            canvas.Zoom = ImClamp(std::expf(log_zoom), MIN_ZOOM, MAX_ZOOM);

            // Snap to 1.0 zoom if close
            if (fabsf(canvas.Zoom - 1.0f) < ZOOM_SNAP_EPSILON)
                canvas.Zoom = 1.0f;

            // Zoom to mouse cursor
            const ImVec2 mouse_pos = io.MousePos - canvas.Pos;
            canvas.Offset = (canvas.Offset - mouse_pos) * (canvas.Zoom / prev_zoom) + mouse_pos;
        }

        // Reset zoom and offset with 'R'
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_R, false))
        {
            canvas.Pos = { 0, 0 };
            canvas.Offset = { 0, 0 };
            canvas.Zoom = 1.0f;
        }

        // Display background, grid and a frame
        if (canvas.Flags & NG_CanvasFlags_DisplayGrid)
        {
            draw_list->AddRectFilled(canvas.Pos, canvas.Pos + canvas_sz, IM_COL32(30, 30, 30, 255));
            const float grid_step = GRID_SIZE * canvas.Zoom;
            if (canvas.Zoom > SUBGRID_ZOOM_THRESHOLD)
            {
                const float subgrid_step = grid_step / GRID_SUBDIVISIONS;
                DrawGrid(canvas.Pos, canvas_sz, canvas.Offset, subgrid_step, GRID_SUBGRID_COLOR);
            }

            DrawGrid(canvas.Pos, canvas_sz, canvas.Offset, grid_step, GRID_COLOR);
            draw_list->AddRect(canvas.Pos, canvas.Pos + canvas_sz, IM_COL32(200, 200, 200, 255));
        }

        // Delete pending nodes and connections
        std::erase_if(canvas.Connections, [](const auto& c) {
            return c.To->ParentNode->MarkedForDeletion || 
                   c.From->ParentNode->MarkedForDeletion;
        });
        std::erase_if(canvas.Nodes, [](const auto& node) {
            return node->MarkedForDeletion == true;
        });

        // Handle new connections
        for (auto& node : canvas.Nodes)
        {
            auto checkPin = [&] (detail::NG_Pin& pin) {
                // Start new connection
                if (!canvas.ActivePin && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && pin.Hovered)
                    canvas.ActivePin = &pin;

                // Try to finish connection
                if (canvas.ActivePin && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && pin.Hovered)
                {
                    detail::NG_Connection connection = { canvas.ActivePin, &pin };
                    if (connection.From->Type == detail::NG_PinType::Input)
                        std::swap(connection.From, connection.To);

                    const bool sameType = canvas.ActivePin->Type == pin.Type;
                    const bool createsCycle = canvas.WouldCreateCycle(connection.From->ParentNode, connection.To->ParentNode);
                    if (sameType)
                        CYB_WARNING("Dropping node connection: {}", "Same pin type");
                    else if (createsCycle)
                        CYB_WARNING("Dropping node connection: {}", "Creates cycles");

                    if (!sameType && !createsCycle)
                    {
                        // Remove any existing connection to this input
                        std::erase_if(canvas.Connections, [&] (const auto& c) {
                            return c.To == connection.To;
                        });

                        canvas.Connections.push_back(connection);

                        // Update connected nodes and send update signal
                        connection.From->ParentNode->ValidState = canvas.NodeHasValidState(connection.From->ParentNode);
                        connection.To->ParentNode->ValidState = canvas.NodeHasValidState(connection.To->ParentNode);
                        connection.To->OnConnect(connection.From);
                        
                        canvas.UpdateAllValidStates();
                        connection.From->ParentNode->ModifiedFlag = true;
                    }
                }
            };

            for (auto& pin : node->Inputs)
                checkPin(*(pin.get()));

            for (auto& pin : node->Outputs)
                checkPin(*(pin.get()));
        }

        // Update hovered node, and move it to the front is clicked
        const bool node_hovered = canvas.UpdateHoveredNode();
        if (node_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            canvas.BringNodeToDisplayFront(canvas.HoveredNode->GetID());

        // Display nodes
        for (auto& node : canvas.Nodes)
        {
            // FIXME: Do we really need to call NodeHasValidState every frame?
            node->ValidState = canvas.NodeHasValidState(node.get());
            
            ImGui::SetWindowFontScale(canvas.Zoom);
            DrawNode(*(node.get()), canvas);
            ImGui::SetWindowFontScale(1.0f);

            if (node->ModifiedFlag)
            {
                canvas.SendUpdateSignalFrom(node.get());
                node->ModifiedFlag = false;
            }
        }

        // Display connections
        bool connection_hovered = false;
        for (auto connection = canvas.Connections.begin(); connection != canvas.Connections.end(); )
        {
            assert(connection->From != nullptr);
            assert(connection->To != nullptr);

            const ImVec2& start = connection->From->Pos;
            const ImVec2& end = connection->To->Pos;
            const float dx = (end.x - start.x) * 0.75f;
            const ImVec2 cp0 = ImVec2(start.x + dx, start.y);
            const ImVec2 cp1 = ImVec2(end.x - dx, end.y);

            // Check if hovering
            const ImVec2 cp = ImBezierCubicClosestPoint(start, cp0, cp1, end, io.MousePos, 20);
            const bool hovered = ImSqrt(ImLengthSqr(cp - io.MousePos)) <= 4.0f * canvas.Zoom;
            connection_hovered |= hovered;

            ImColor col = hovered ? style.ConnectionHoverColor : style.ConnectionColor;
            draw_list->AddBezierCubic(start, cp0, cp1, end, col, 2.1f * canvas.Zoom);

            // Right click to delete
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                connection->To->OnConnect(nullptr);
                connection = canvas.Connections.erase(connection);
                canvas.ConnectionClick = true;
            }
            else
                ++connection;
        }

        // Display any ongoing connection
        if (canvas.ActivePin && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const ImVec2& start = canvas.ActivePin->Pos;
            const ImVec2& end = io.MousePos;
            const float dx = (end.x - start.x) * 0.75f;
            const ImVec2 cp0 = ImVec2(start.x + dx, start.y);
            const ImVec2 cp1 = ImVec2(end.x - dx, end.y);

            draw_list->AddBezierCubic(start, cp0, cp1, end, style.ConnectionDragginColor, 2.0f * canvas.Zoom);
        }

        if (canvas.ActivePin && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            canvas.ActivePin = nullptr;

        // Display factory menu
        const bool any_hovered = node_hovered || connection_hovered;
        const bool popup_open = ImGui::IsPopupOpen(ImGui::GetID("#FACTORY_POPUP"), 0);
        const bool can_display_popup = (!any_hovered || popup_open) && !canvas.ConnectionClick;
        if (can_display_popup && canvas.Factory && ImGui::BeginPopupContextWindow("#FACTORY_POPUP", ImGuiPopupFlags_MouseButtonRight))
        {
            const ImVec2 window_mouse_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
            const ImVec2 canvas_mouse_pos = (window_mouse_pos - canvas.Pos - canvas.Offset) / canvas.Zoom;
            
            ImGui::Text("Create New Node");
            ImGui::Separator();
            canvas.Factory->DrawMenuContent(canvas, canvas_mouse_pos);
            ImGui::EndPopup();
        }

        if (ImGui::GetActiveID() == canvas_id)
            ImGui::ClearActiveID();

#ifndef CYB_DEBUG_NG_NODE_STATE
        const bool display_state = (canvas.Flags & NG_CanvasFlags_DisplayState);
#else
        const bool display_state = true;
#endif
        if (display_state)
        {
            ImGui::SetCursorScreenPos(canvas.Pos + ImVec2(8, 8));
            ImGui::BeginGroup();
            ImGui::Text("Offset: [%.1f, %.1f]", canvas.Offset.x, canvas.Offset.y);
            ImGui::Text("Zoom: %.2f", canvas.Zoom);
#ifdef CYB_DEBUG_NG_NODE_STATE
            ImGui::Text("Canvas pos: [%.1f, %.1f]", canvas.Pos.x, canvas.Pos.y);
            ImGui::Text("Hovered nodeID: %u", canvas.HoveredNode ? canvas.HoveredNode->GetID() : 0);
            ImGui::Text("Node count: %u", canvas.Nodes.size());
            ImGui::Text("Connection count: %u", canvas.Connections.size());
#endif
            ImGui::EndGroup();
        }

        if (canvas.ConnectionClick && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            canvas.ConnectionClick = false;

        ImGui::PopID();
        return true;
    }

    void NodeGraphStyleEditor(NG_Canvas& canvas)
    {
        NG_Style& style = canvas.Style;

        if (ImGui::Begin("NG_StyleEdit"))
        {
            ImGui::SliderFloat("PinRadius", &style.PinRadius, 1.0f, 12.0f);
            ImGui::SliderFloat("NodeFrameRounding", &style.NodeFrameRounding, 1.0f, 12.0f);
            ImGui::SliderFloat2("NodeWindowPadding", (float*)&style.NodeWindowPadding, 1.0f, 12.0f);
            ImGui::SliderFloat2("NodeFramePadding", (float*)&style.NodeFramePadding, 1.0f, 12.0f);
            ImGui::ColorEdit4("NodeBackgroundColor", (float*)&style.NodeBackgroundColor);
            ImGui::ColorEdit4("NodeBorderColor", (float*)&style.NodeBorderColor);
            ImGui::ColorEdit4("NodeBorderInvalidColor", (float*)&style.NodeBorderInvalidColor);
            ImGui::ColorEdit4("PinUnConnectedColor", (float*)&style.PinUnConnectedColor);
            ImGui::ColorEdit4("PinConnectedColor", (float*)&style.PinConnectedColor);
            ImGui::ColorEdit4("PinHoverColor", (float*)&style.PinHoverColor);
            ImGui::ColorEdit4("ConnectionColor", (float*)&style.ConnectionColor);
            ImGui::ColorEdit4("ConnectionHoverColor", (float*)&style.ConnectionHoverColor);
            ImGui::ColorEdit4("ConnectionDragginColor", (float*)&style.ConnectionDragginColor);

            if (ImGui::Button("Set To Defaults"))
                style = ui::NG_Style{};
        }

        ImGui::End();
    }
}