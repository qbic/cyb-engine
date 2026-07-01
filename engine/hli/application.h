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
        [[nodiscard]] RenderPath* GetActivePath() { return activePath; }

        virtual void Init();
        void UpdateLoop();
        virtual void Update(double dt);
        virtual void Render();
        virtual void Compose(rhi::CommandList cmd);

        // implemented by game, returned object must be kept alive until application exit
        [[nodiscard]] virtual RenderPath* GetRenderPath() const = 0;

        // Call this before calling Run() or Initialize() if you want to render to a UWP window
        void SetWindow(WindowHandle window);
        [[nodiscard]] WindowHandle GetWindow() const { return window; }

    private:
        bool isWindowActive = true;
        std::unique_ptr<rhi::GraphicsDevice> graphicsDevice;
        eventsystem::Handle changeVSyncEvent;
        bool initialized = false;
        Timer timer;
        double deltaTime = 0.0;
        Canvas canvas;
        RenderPath* activePath = nullptr;
        ClientWindow m_window{};
        WindowHandle window;            // TODO: REMOVE
        rhi::Swapchain swapchain;
    };
}
