#pragma once
#include "core/platform.h"
#include "core/timer.h"
#include "hli/renderpath.h"

namespace cyb::hli
{
    class Application
    {
    public:
        virtual ~Application() = default;

        void ActivePath(RenderPath* component);
        RenderPath* GetActivePath() { return activePath; }

        void Run();
        virtual void Initialize();
        virtual void Update(double dt);
        virtual void Render();
        virtual void Compose(graphics::CommandList cmd);

        // Call this before calling Run() or Initialize() if you want to render to a UWP window
        void SetWindow(platform::WindowType window);
        platform::WindowType GetWindow() const { return window; }

        void KillWindowFocus() { isWindowActive = false; }
        void SetWindowFocus() { isWindowActive = true; }
        [[nodiscard]] bool IsWindowActive() const { return isWindowActive; }

    private:
        bool isWindowActive = true;
        std::unique_ptr<graphics::GraphicsDevice> graphicsDevice;
        eventsystem::Handle changeVSyncEvent;
        bool initialized = false;
        Timer timer;
        double deltaTime = 0.0; 
        Canvas canvas;
        RenderPath* activePath = nullptr;
        platform::WindowType window;
        graphics::SwapChain swapchain;
    };
}
