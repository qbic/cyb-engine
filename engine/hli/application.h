#pragma once
#include "core/sys.h"
#include "core/timer.h"
#include "hli/renderpath.h"
#include "graphics/window.h"

namespace cyb::hli
{
    class Application
    {
    public:
        virtual ~Application() = default;

        void ActivePath(RenderPath* component);
        [[nodiscard]] RenderPath* GetActivePath() { return m_activePath; }

        virtual void Init();
        void UpdateLoop();
        virtual void Update(double dt);
        virtual void Render();
        virtual void Compose(rhi::CommandList cmd);

        // implemented by game, returned object must be kept alive until application exit
        [[nodiscard]] virtual RenderPath* GetRenderPath() const = 0;

        /** Returns a reference to the client window. */
        [[nodiscard]] const ClientWindow& GetWindow() const noexcept { return m_window; }

        /** 
         * Rebuilds the swapchain and recreates all render targets. This uses the client
         * window and render path renderScale to determine the new swapchain size.
         */
        void RebuildSwapchain();

    private:
        void InitGraphicsDevice();

        std::unique_ptr<rhi::GraphicsDevice> m_graphicsDevice{};
        eventsystem::Handle m_changeVSyncEvent{};
        Timer m_timer{};
        RenderPath* m_activePath{ nullptr };
        ClientWindow m_window{};
        rhi::Swapchain swapchain{};
    };
}
