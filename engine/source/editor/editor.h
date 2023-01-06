#pragma once
#include <unordered_set>
#include "systems/scene.h"

namespace cyb::editor::gui
{
    bool CheckBox(const std::string& label, bool& value);
    bool DragFloat(const std::string& label, float& value, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.2f");
    bool DragInt(const std::string& label, uint32_t& value, float v_speed = 1.0f, uint32_t v_min = 0, uint32_t v_max = 0, const char* format = "%d");
    bool SliderFloat(const std::string& label, float& value, float v_min = 0.0f, float v_max = 0.0f);
    bool SliderInt(const std::string& label, uint32_t& value, uint32_t v_min = 0.0f, uint32_t v_max = 0.0f, const char* format = "%d");
    bool ComboBox(const std::string& label, uint32_t* selected_index, const std::vector<std::string>& combo_data);
}

namespace cyb::editor 
{
    class GuiTool
    {
    public:
        GuiTool(const std::string& name) { Init();  window_title = name; }
        virtual ~GuiTool() = default;

        const char* GetWindowTitle() const { return window_title.c_str(); }
        void ShowWindow(bool show = true) { show_window = show; }
        bool IsShown() const { return show_window; }

        bool PreDraw();
        void PostDraw();

        virtual void Init() {}
        virtual void Draw() = 0;

    private:
        std::string window_title;
        bool show_window = false;
    };

    class SceneGraphView
    {
    private:
        struct Node
        {
            Node() = default;
            Node(Node* _parent, ecs::Entity _entity, const std::string_view& _name) :
                entity(_entity), name(_name), parent(_parent)
            {
            }
             
            ecs::Entity entity = ecs::InvalidEntity;
            const std::string_view name;
            Node* parent = nullptr;
            std::vector<Node> children;
        };

        Node root;
        std::atomic<ecs::Entity> selected_entity = ecs::InvalidEntity;
        std::unordered_set<ecs::Entity> added_entities;

        void AddNode(Node* parent, ecs::Entity entity, const std::string_view& name);
        void DrawNode(const Node* node);

    public:
        void GenerateView();
        void Draw();
        void SelectEntity(ecs::Entity entity) { selected_entity = entity; }
        ecs::Entity SelectedEntity() const { return selected_entity; }
        bool IsSelected(ecs::Entity entity) const { return entity == selected_entity; }
    };

    void Initialize();
    void SetSceneGraphViewSelection(ecs::Entity entity);
    void PushFrameTime(const float t);
    void Update();

    // Check whether the editor wants the input (key or mouse)
    bool WantInput();
}
