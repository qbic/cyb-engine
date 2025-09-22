#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <ranges>
#include <variant>
#define IMGUI_DEFINE_MATH_OPERATORS     // TODO: Remove when moving stuff to .cpp
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
        PushID(const void* id)
        {
            ImGui::PushID(id);
        }
        PushID(const char* id)
        {
            ImGui::PushID(id);
        }
        PushID(int id)
        {
            ImGui::PushID(id);
        }
        ~PushID()
        {
            ImGui::PopID();
        }
    };

    //------------------------------------------------------------------------------

    // Draw an info icon with mouse over text
    void InfoIcon(const char* fmt, ...);

    // These are basicly just ImGui wrappers wich uses left aligned labels, 
    // and saves any change to the undo manager
    bool Checkbox(const char* label, bool* v, const std::function<void()> onChange);
    void CheckboxFlags(const char* label, uint32_t* flags, uint32_t flagsValue, const std::function<void()> onChange);
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderFloat(const char* label, float* v, const std::function<void()> onChange, float minValue, float maxValue, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v, const std::function<void()> onChange, int minValue, int maxValue, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
    bool EditTransformComponent(const char* label, float component[3], scene::TransformComponent* transform);

    void ComboBox(const char* label, uint32_t& value, const std::unordered_map<uint32_t, std::string>& combo, const std::function<void()>& onChange = nullptr);

    template <typename T,
        typename std::enable_if < std::is_enum<T>{} ||
        std::is_integral<std::underlying_type_t<T>>{}, bool > ::type = true >
    void ComboBox(const char* label, T & value, const std::unordered_map<T, std::string>&combo, const std::function<void()>&onChange = nullptr)
    {
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

    /*-----------------------------------------------------------------------------
     * Node graph functions
     *
     * Usage:
     *      * Left mouse button to drag a node.
     *      * Scrollwheel to zoom canvas.
     *      * Drag with middle mouse button to scroll canvas.
     *      * Press 'R' to reset scroll, position and zoom.
     *      * Drag with left mouse button from pin to pin to create connection.
     *      * Right-click connection to remove.
     *
     * Connection rules:
     *      * Only one connection per input pin.
     *        Prevous connection will be removed if new is created.
     *      * Output pins can have multiple connections.
     *      * You can not connect an output to an input in the same node.
     *      * You can not create cyclic (infinite) loop connections.
     *      * You can not connect input to input, or output to output.
     *------------------------------------------------------------------------------*/

     // Comment out to draw node graph debug helpers
 //#define CYB_DEBUG_NODE_GRAPH_RECTS
 //#define CYB_DEBUG_NODE_GRAPH_STATE

    enum NG_CanvasFlags
    {
        NG_CanvasFlags_None = 0,
        NG_CanvasFlags_DisplayGrid = 1 << 0,
        NG_CanvasFlags_DisplayState = 1 << 1
    };

    struct NG_Node;

    namespace detail
    {
        template <typename T>
        struct NG_OutputPin;

        enum class NG_PinType
        {
            Input,
            Output
        };

        struct NG_Pin
        {
            std::string Label;

            // Internal data (don't manually set)
            bool Hovered{ false };
            NG_PinType Type{ NG_PinType::Input };
            NG_Node* ParentNode{ nullptr };
            ImVec2 Pos{ 0, 0 };

            virtual ~NG_Pin() = default;
            virtual void Connect(detail::NG_Pin*) = 0;
        };

        template <typename T>
        struct NG_InputPin : public detail::NG_Pin
        {
            using CallbackType = std::function<void(std::optional<T>)>;
            CallbackType Callback;

            void Connect(detail::NG_Pin* from) override
            {
                auto* out = dynamic_cast<NG_OutputPin<T>*>(from);
                if (out && out->Callback)
                    Callback(out->Callback());
                else
                    Callback(std::nullopt);
            }
        };

        template <typename T>
        struct NG_OutputPin : public detail::NG_Pin
        {
            using CallbackType = std::function<T()>;
            CallbackType Callback;

            // Only connect from InputPin!
            void Connect(detail::NG_Pin*) override
            {
                assert(0);
            }
        };

        struct NG_Connection
        {
            detail::NG_Pin* From;       // OutputPin
            detail::NG_Pin* To;         // InputPin
        };
    }

    struct NG_Node
    {
        ImVec2 Pos{ 0, 0 };             // Position, relative to cavas
        ImVec2 Size{ 1, 1 };
        std::vector<std::unique_ptr<detail::NG_Pin>> Inputs;
        std::vector<std::unique_ptr<detail::NG_Pin>> Outputs;
        bool ValidState{ false };
        bool ModifiedFlag{ false };
        bool MarkedForDeletion{ false };

        NG_Node(const std::string& label)
        {
            m_Label = label;
            m_ID = ImGui::GetID(this);
        }

        const std::string& GetLabel() const
        {
            return m_Label;
        }
        ImGuiID GetID() const
        {
            return m_ID;
        }

        // This may be implemented in derived class for displaying custom items.
        virtual void DisplayContent(float zoom)
        {
        }
        virtual void Update()
        {
        }

        template <typename T>
        void AddInputPin(const std::string& label, detail::NG_InputPin<T>::CallbackType cb = {})
        {
            auto pin = std::make_unique<detail::NG_InputPin<T>>();
            pin->Label = label;
            pin->Type = detail::NG_PinType::Input;
            pin->ParentNode = this;
            pin->Callback = std::move(cb);
            Inputs.push_back(std::move(pin));
        }

        template <typename T>
        void AddOutputPin(const std::string& label, detail::NG_OutputPin<T>::CallbackType cb = {})
        {
            auto pin = std::make_unique<detail::NG_OutputPin<T>>();
            pin->Label = label;
            pin->Type = detail::NG_PinType::Output;
            pin->ParentNode = this;
            pin->Callback = std::move(cb);
            Outputs.push_back(std::move(pin));
        }

    private:
        std::string m_Label;
        ImGuiID m_ID{ 0 };
    };

    class NG_Factory
    {
    public:
        NG_Factory() = default;
        ~NG_Factory() = default;

        using NodeCreator = std::function<std::unique_ptr<NG_Node>()>;

        struct Entry
        {
            bool isSeperator = false;
            NodeCreator creator;
        };

        void Register(const std::string& name, NodeCreator creator)
        {
            Entry entry = {};
            entry.creator = std::move(creator);
            m_creators[name] = std::move(entry);
        }

        std::unique_ptr<NG_Node> Create(const std::string& name) const
        {
            auto it = m_creators.find(name);
            if (it != m_creators.end())
                return it->second.creator();
            return nullptr;
        }

        const std::unordered_map<std::string, Entry>& GetRegistry() const
        {
            return m_creators;
        }

    private:
        std::unordered_map<std::string, Entry> m_creators;
    };

    struct NG_Canvas
    {
        ImVec2 Pos{ 0.0f, 0.0f };
        ImVec2 Offset{ 0.0f, 0.0f };                    // Canvas scrolling offset
        float Zoom{ 1.0f };
        NG_CanvasFlags Flags{ NG_CanvasFlags_None };
        NG_Factory Factory;
        std::vector<std::unique_ptr<NG_Node>> Nodes;    // Nodes, sorted in display order, back to front
        std::vector<detail::NG_Connection> Connections;
        detail::NG_Pin* ActivePin{ nullptr };
        const NG_Node* HoveredNode{ nullptr };

        // FIXME: This is used to track if a click is used to delete a connection
        //        so that factory popup does not open on the same click.
        bool ConnectionClick{ false };

        /**
         * @brief Check if there are any connection to the pin.
         */
        [[nodiscard]] bool IsPinConnected(detail::NG_Pin* pin) const;

        /**
         * @brief Check if a connection would create cycling connections.
         */
        [[nodiscard]] bool WouldCreateCycle(const NG_Node* from, const NG_Node* to) const;

        /**
         * @brief Search for a connection that connects to pin.
         * @return A pointer to the connection if any or nullptr otherwise.
         */
        [[nodiscard]] const detail::NG_Connection* FindConnectionTo(detail::NG_Pin* pin) const;

        /**
         * @brief Search for connections that connect from pin.
         * @return A range filter view of connections.
         */
        [[nodiscard]] std::vector<const detail::NG_Connection*> FindConnectionsFrom(detail::NG_Pin* pin) const;

       /**
        * @brief Updates the ValidState on all nodes. See NG_Canvas::NodeHasValidState().
        */
        void UpdateAllValidStates();

        /**
         * @brief Check if node, and all dependent nodes have all input pins connected.
         */
        [[nodiscard]] bool NodeHasValidState(NG_Node* node) const;

        /**
         * @brief Send an update signal calling NG_Node::Update() to all parents connected to node.
         */
        void SendUpdateSignalFrom(NG_Node* node);

        /**
         * @brief Update hovered node.
         *
         * Searching display front to back to get the top-most hovered node.
         * If no node is hovered, HoveredNode is set to nullptr.
         * @return True if any node is hovered, False otherwise.
         */
        bool UpdateHoveredNode();

        /**
         * @brief Check if node, by id, is the top-most hovered node.
         */
        [[nodiscard]] bool IsNodeHovered(ImGuiID node_id) const;

        /**
         * @brief Find node by node id and bring it to the front.
         */
        void BringNodeToDisplayFront(ImGuiID node_id);
    };

    bool NodeGraph(NG_Canvas& canvas);
}