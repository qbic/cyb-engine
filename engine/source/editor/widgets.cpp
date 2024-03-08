/*
 * Gradient editor based on:
 * David Gallardo's https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112
 */
#include <memory>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "editor/widgets.h"
#include "imgui/imgui_internal.h"

namespace cyb::ui {

    ScopedStyleColor::ScopedStyleColor(ImGuiCol id, const ImVec4& color) :
        numColors(1) {
        ImGui::PushStyleColor(id, color);
    }

    ScopedStyleColor::ScopedStyleColor(ImGuiCol id, ImColor color) :
        numColors(1) {
        ImGui::PushStyleColor(id, color.Value);
    }

    ScopedStyleColor::ScopedStyleColor(const std::initializer_list<std::pair<ImGuiCol, ImVec4>> colors) :
        numColors(static_cast<uint32_t>(colors.size())) {
        for (const auto& [id, color] : colors) {
            ImGui::PushStyleColor(id, color);
        }
    }

    ScopedStyleColor::ScopedStyleColor(const std::initializer_list<std::pair<ImGuiCol, ImU32>> colors) :
        numColors(static_cast<uint32_t>(colors.size())) {
        for (const auto& [id, color] : colors) {
            ImGui::PushStyleColor(id, color);
        }
    }

    ScopedStyleColor::~ScopedStyleColor() {
        ImGui::PopStyleColor(numColors);
    }

    void ItemLabel(const std::string& title, bool isLeft = true)
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
        //textRect.Max.x += fullWidth - itemWidth;
        textRect.Max.x += itemWidth * 0.5f;
        textRect.Max.y += textSize.y;

        ImGui::SetCursorScreenPos(textRect.Min);

        ImGui::AlignTextToFramePadding();
        // Adjust text rect manually because we render it directly into a drawlist instead of using public functions.
        textRect.Min.y += window.DC.CurrLineTextBaseOffset;
        textRect.Max.y += window.DC.CurrLineTextBaseOffset;

        ImGui::ItemSize(textRect);
        if (ImGui::ItemAdd(textRect, window.GetID(title.data(), title.data() + title.size())))
        {
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
        else
        {
            ImGui::SetCursorScreenPos(lineStart);
        }
    }

    // Record any change to value v to the undo-manager, if value is
    // changed directly or though undo-manager onChanger() will be called
    template <class T, typename... Args>
    void SaveChangeToUndoManager(typename T::value_type* value, [[maybe_unused]] Args&&... args) {
        if (ImGui::IsItemActivated()) {
            ui::GetUndoManager().Emplace<T>(value, std::forward<Args>(args)...);
        }

        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) {
            ui::GetUndoManager().ClearIncompleteCommand();
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ui::GetUndoManager().CommitIncompleteCommand();
        }
    }

#define COMMON_WIDGET_CODE(label)   \
    ScopedID m_idGuard(label);  \
    ItemLabel(label);               \
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    void Checkbox(const char* label, bool* v, const std::function<void()> onChange)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::Checkbox("", v);
        ImGui::PopStyleVar();
        SaveChangeToUndoManager<ui::ModifyValue<bool, 1>>(v, onChange);
    }

    void CheckboxFlags(const char* label, uint32_t* flags, uint32_t flagsValue, const std::function<void()> onChange)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::CheckboxFlags("", (unsigned int*)flags, (unsigned int)flagsValue);
        ImGui::PopStyleVar();
        SaveChangeToUndoManager<ui::ModifyValue<uint32_t, 1>>(flags, onChange);
    }

    bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        bool change = ImGui::DragFloat("", v, v_speed, v_min, v_max, format, flags);
        ImGui::PopStyleVar();
        SaveChangeToUndoManager<ui::ModifyValue<float, 1>>(v);
        return change;
    }
    
    bool DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        bool change = ImGui::DragFloat3("", v, v_speed, v_min, v_max, format, flags);
        ImGui::PopStyleColor();
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

    void SliderFloat(const char* label, float* v, const std::function<void()> onChange, float minValue, float maxValue, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        float temp = *v;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::SliderFloat("", &temp, minValue, maxValue);
        ImGui::PopStyleVar();
        SaveChangeToUndoManager<ui::ModifyValue<float, 1>>(v, onChange);
        *v = temp;
    }

    void SliderInt(const char* label, int* v, const std::function<void()> onChange, int minValue, int maxValue, const char* format, ImGuiSliderFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        int temp = *v;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::SliderInt("", &temp, minValue, maxValue, format, flags);
        ImGui::PopStyleVar();

        SaveChangeToUndoManager<ui::ModifyValue<int, 1>>(v, onChange);
        *v = temp;
    }

    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        bool change = ImGui::ColorEdit3("", col, flags);
        ImGui::PopStyleVar();

        SaveChangeToUndoManager<ui::ModifyValue<float, 3>>(col);
        return change;
    }

    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
    {
        COMMON_WIDGET_CODE(label);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        bool change = ImGui::ColorEdit4("", col, flags);
        ImGui::PopStyleVar();

        SaveChangeToUndoManager<ui::ModifyValue<float, 4>>(col);
        return change;
    }

    bool EditTransformComponent(const char* label, float component[3], scene::TransformComponent* transform)
    {
        // Componenet must reside within TransformComponent!
        assert((uint8_t*)component >= (uint8_t*)transform && (uint8_t*)component < ((uint8_t*)transform + sizeof(*transform)));
        COMMON_WIDGET_CODE(label);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        bool change = ImGui::DragFloat3("", component, 0.1f, 0.0f, 0.0f, "%.1f");
        ImGui::PopStyleVar();

        if (change) {
            transform->SetDirty();
        }
        SaveChangeToUndoManager<ui::ModifyTransform>(transform);

        return change;
    }

    void ComboBox(const char* label, uint32_t& value, const std::unordered_map<uint32_t, std::string>& combo, const std::function<void()>& onChange)
    {
        COMMON_WIDGET_CODE(label);
        bool valueChange = false;
        const auto& selected = combo.find(value);
        const std::string& name = selected != combo.end() ? selected->second : "";

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        if (ImGui::BeginCombo("", name.c_str()))
        {
            for (const auto& [key, name] : combo)
            {
                const bool isSelected = (key == value);
                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    const uint32_t tempValue[1] = { key };
                    ui::GetUndoManager().Emplace<ui::ModifyValue<uint32_t, 1>>(&value, tempValue, onChange);

                    value = key;
                    valueChange = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        ImGui::PopStyleVar();

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
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
            change = ImGui::ListBox("", selectedIndex, getter, static_cast<void*>(&values), (int)values.size());
            ImGui::PopStyleVar();
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
        markList.sort([](const GradientMark* a, const GradientMark* b)
            {
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
        if (!ImGui::ItemAdd(bb, ImGui::GetItemID()))
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
}