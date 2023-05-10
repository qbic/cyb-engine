#pragma once
#include "core/platform.h"
#include "core/timer.h"
#include "hli/render-path.h"

namespace cyb::hli
{
    class Application
    {

    public:
        virtual ~Application() = default;

        void ActivePath(RenderPath* component);
        RenderPath* GetActivePath() { return active_path; }

        void Run();
        virtual void Initialize();
        virtual void Update(double dt);
        virtual void Render();
        virtual void Compose(graphics::CommandList cmd);

        // Call this before calling Run() or Initialize() if you want to render to a UWP window
        void SetWindow(std::shared_ptr<platform::Window> window);

    private:
        std::unique_ptr<graphics::GraphicsDevice> graphics_device;
        eventsystem::Handle change_vsyc_event;
        bool initialized = false;
        Timer timer;
        double m_deltaTime = 0.0;
        RenderPath* active_path = nullptr;
        std::shared_ptr<platform::Window> window;
        graphics::SwapChain swapchain;

    };
}
