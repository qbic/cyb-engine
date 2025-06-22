#pragma once
#include <unordered_set>
#include "systems/scene.h"

namespace cyb::editor 
{
    class ToolWindow
    {
    public:
        ToolWindow(const std::string& name, int windowFlags = 0) :
            m_windowTitle(name),
            m_isVisible(false),
            m_windowFlags(windowFlags)
        {
            Init();
        }
        virtual ~ToolWindow() = default;

        const char* GetWindowTitle() const { return m_windowTitle.c_str(); }
        void SetVisible(bool visible) { m_isVisible = visible; }
        bool IsVisible() const { return m_isVisible; }
        bool IsHidden() const { return !m_isVisible; }
        void Draw();

        virtual void Init() {}
        virtual void WindowContent() = 0;

    private:
        std::string m_windowTitle;
        bool m_isVisible;
        int m_windowFlags;
    };

    class SceneGraphView
    {
    public:
        void GenerateView();
        void WindowContent();
        void SetSelectedEntity(ecs::Entity entity);
        ecs::Entity GetSelectedEntity() const { return m_selectedEntity; }

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

        Node m_root;
        std::atomic<ecs::Entity> m_selectedEntity = ecs::INVALID_ENTITY;
        std::unordered_set<ecs::Entity> m_entities;

        void AddNode(Node* parent, ecs::Entity entity, const std::string_view& name);
        void DrawNode(const Node* node);
    };

    void Initialize();
    void Update();

    // NOTE: need to be called from RenderPath3D
    void Render(const rhi::Texture* rtDepthStencil, rhi::CommandList cmd);

    void UpdateFPSCounter(double dt);

    // Check whether the editor wants the input (key or mouse)
    bool WantInput();
}
