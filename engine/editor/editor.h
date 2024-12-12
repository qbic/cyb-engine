#pragma once
#include <unordered_set>
#include "systems/scene.h"

namespace cyb::editor 
{
    class GuiTool
    {
    public:
        GuiTool(const std::string& name, int windowFlags = 0) :
            m_windowTitle(name),
            m_showWindow(false),
            m_windowFlags(windowFlags)
        {
            Init();
        }
        virtual ~GuiTool() = default;

        const char* GetWindowTitle() const { return m_windowTitle.c_str(); }
        void ShowWindow(bool show = true) { m_showWindow = show; }
        bool IsShown() const { return m_showWindow; }

        virtual bool PreDraw();
        virtual void PostDraw();

        virtual void Init() {}
        virtual void Draw() = 0;

    private:
        std::string m_windowTitle;
        bool m_showWindow;
        int m_windowFlags;
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
             
            ecs::Entity entity = ecs::INVALID_ENTITY;
            const std::string_view name;
            Node* parent = nullptr;
            std::vector<Node> children;
        };

        Node root;
        std::atomic<ecs::Entity> selected_entity = ecs::INVALID_ENTITY;
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
    void Update();

    void UpdateFPSCounter(double dt);

    // Check whether the editor wants the input (key or mouse)
    bool WantInput();
}
