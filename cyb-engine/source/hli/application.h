#pragma once
#include "Core/Platform.h"
#include "Core/Timer.h"
#include "hli/render-path.h"

namespace cyb::hli
{
    class Application
    {
    private:
        std::unique_ptr<graphics::GraphicsDevice> graphics_device;
        eventsystem::Handle change_vsyc_event;
        bool initialized = false;
        Timer timer;
        float delta_time = 0.0f;
        RenderPath* active_path = nullptr;
        std::shared_ptr<platform::Window> window;
        graphics::SwapChain swapchain;

    public:
        virtual ~Application() = default;

        void ActivePath(RenderPath* component);
        RenderPath* GetActivePath() { return active_path; }

        void Run();
        virtual void Initialize();
        virtual void Update(float dt);
        virtual void Render();
        virtual void Compose(graphics::CommandList cmd);

        // Call this before calling Run() or Initialize() if you want to render to a UWP window
        void SetWindow(std::shared_ptr<platform::Window> window);
    };
}
