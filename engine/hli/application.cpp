#include "core/cvar.h"
#include "graphics/device_vulkan.h"
#include "graphics/renderer.h"
#include "systems/event_system.h"
#include "systems/job_system.h"
#include "systems/profiler.h"
#include "systems/resource_manager.h"
#include "input/input.h"
#include "hli/application.h"
#include "config.h"

#include "editor/imgui_backend.h"
#include "editor/filedialog.h"

#include <chrono>

using namespace cyb::rhi;

namespace cyb::hli
{
    CVar<bool> r_vsync{ "r_vsync", true, CVarFlag::RendererBit, "Enable/Disable framebuffer swap on vertical refresh" };
    CVar<uint32_t> r_maxBackgroundFps{ "r_maxBackgroundFps", 30, 1, 240, CVarFlag::RendererBit, "Maximum FPS when the application is not active" };
    
    void Application::ActivePath(RenderPath* component)
    {
        if (component != nullptr)
            component->SetCanvas(canvas);
        activePath = component;
    }

    void Application::Init()
    {
        Timer timer;
#ifdef CYB_DEBUG_BUILD
        CYB_INFO("Running debug build, performance will be slow!");
#endif
        CYB_INFO_HR();
        CYB_INFO("Initializing cyb-engine v{} (asynchronous), please wait...", CYB_VERSION_STRING);

        RegisterStaticCVars();
        jobsystem::Initialize();

        m_window = ClientWindow::Create({ });
        SetWindow(m_window.GetNativeHandle().hWnd);       // TODO: REMOVE

        jobsystem::Context ctx;
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { resourcemanager::Initialize(); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) { input::Initialize(window); });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { renderer::Initialize(); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) { ImGui_Impl_CybEngine_Init(window); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) {
            RenderPath* renderPath = GetRenderPath();
            renderPath->Load();

            ActivePath(renderPath);
        });
        jobsystem::Wait(ctx);

        CYB_INFO("cyb-engine initialized in {:.2f}ms ", timer.ElapsedMilliseconds());
        CYB_INFO_HR();
    }

    void Application::UpdateLoop()
    {
        do 
        {
            // Update input state and poll window events
            input::Update(window);
            if (!m_window.PollEvents())
                break;

            // Add a little delay to relax the system if the client window is not
            // active, and skip update and rendering entierly is it's minimized.
            if (!m_window.IsActive())
            {
                const double TargetFrameTimeMs = 1000.0 / double(r_maxBackgroundFps.GetValue());
                const double elapsedMs = timer.ElapsedMilliseconds();
                const auto remainingTime = std::chrono::duration<double, std::milli>(TargetFrameTimeMs - elapsedMs);
                if (remainingTime > std::chrono::duration<double, std::milli>::zero())
                    std::this_thread::sleep_for(remainingTime);

                if (m_window.IsMinimized())
                    continue;
            }

            // Rebuild swapchain and update buffers if client window changes size
            if (m_window.GetWidth() != activePath->GetPhysicalWidth() ||
                m_window.GetHeight() != activePath->GetPhysicalHeight())
                SetWindow(m_window.GetNativeHandle().hWnd);

            profiler::BeginFrame();
            float dt = timer.RecordElapsedSeconds();

            // Wake up the events that need to be executed on the main thread, 
            // in thread safe manner.
            eventsystem::FireEvent(eventsystem::Event_ThreadSafePoint, 0);

            Update(dt);

            Render();

            // Compose the final image and and pass it to the swapchain for display
            CommandList cmd = graphicsDevice->BeginCommandList();
            graphicsDevice->BeginRenderPass(&swapchain, cmd);
            Viewport viewport{};
            viewport.width = (float)swapchain.GetDesc().width;
            viewport.height = (float)swapchain.GetDesc().height;
            graphicsDevice->BindViewports(&viewport, 1, cmd);

            Compose(cmd);
            graphicsDevice->EndRenderPass(cmd);

            profiler::EndFrame(cmd);
            graphicsDevice->ExecuteCommandLists();
        } while (true);
    }

    void Application::Update(double dt)
    {
        CYB_PROFILE_CPU_SCOPE("Update");
        if (activePath != nullptr)
            activePath->Update(dt);
    }

    void Application::Render()
    {
        CYB_PROFILE_CPU_SCOPE("Render");
        if (activePath != nullptr)
            activePath->Render();
    }

    void Application::Compose(rhi::CommandList cmd)
    {
        CYB_PROFILE_CPU_SCOPE("Compose");
        if (activePath != nullptr)
            activePath->Compose(cmd);
    }

    void Application::SetWindow(WindowHandle window)
    {
        this->window = window;

        // this should probably be handled by the editor separately
        SetFileDialogParentWindow(window);

        if (graphicsDevice == nullptr)
        {
            graphicsDevice = std::make_unique<rhi::GraphicsDevice_Vulkan>();
            rhi::GetDevice() = graphicsDevice.get();
            rhi::GraphicsDevice* device = rhi::GetDevice();
        }

        canvas.SetCanvas(window);

        rhi::SwapchainDesc desc = {};
        WindowInfo info = GetWindowInfo(window);
        desc.width = canvas.GetPhysicalWidth();
        desc.height = canvas.GetPhysicalHeight();;
        rhi::GetDevice()->CreateSwapchain(&desc, window, &swapchain);

        changeVSyncEvent = eventsystem::Subscribe(eventsystem::Event_SetVSync, [this] (uint64_t userdata) {
            SwapchainDesc desc = swapchain.desc;
            desc.vsync = (userdata != 0);
            bool success = graphicsDevice->CreateSwapchain(&desc, nullptr, &swapchain);
            assert(success);
        });

        r_vsync.ClearCallbacks();
        r_vsync.RegisterOnChangeCallback([] (const CVar<bool>& cvar) {
            eventsystem::FireEvent(eventsystem::Event_SetVSync, cvar.GetValue() ? 1ull : 0ull);
        });
    }
}