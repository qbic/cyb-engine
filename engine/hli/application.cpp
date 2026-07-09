#include <chrono>
#include <functional>
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

using namespace cyb::rhi;

namespace cyb::hli
{
    CVar<bool> r_vsync{ "r_vsync", true, CVarFlag::RendererBit, "Enable/Disable framebuffer swap on vertical refresh" };
    CVar<uint32_t> r_maxBackgroundFps{ "r_maxBackgroundFps", 30, 1, 240, CVarFlag::RendererBit, "Maximum FPS when the application is not active" };
    
    void Application::ActivePath(RenderPath* component)
    {
        if (component != nullptr)
            component->SetCanvas(m_window.GetWidth(), m_window.GetHeight());
        m_activePath = component;
    }

    void Application::Init()
    {
        Timer timer{};
#ifdef CYB_DEBUG_BUILD
        CYB_INFO("Running debug build, performance will be slow!");
#endif
        CYB_INFO_HR();
        CYB_INFO("Initializing cyb-engine v{} (asynchronous), please wait...", CYB_VERSION_STRING);

        RegisterStaticCVars();
        jobsystem::Initialize();

        // Initialize the client window, graphics device, and swapchain
        m_window = ClientWindow::Create({ });
        InitGraphicsDevice();
        RebuildSwapchain();

        jobsystem::Context ctx{};
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { resourcemanager::Initialize(); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) { input::Initialize(m_window.GetNativeHandle()); });
        jobsystem::Execute(ctx, [] (jobsystem::JobArgs) { renderer::Initialize(); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) { ImGui_Impl_CybEngine_Init(m_window.GetNativeHandle()); });
        jobsystem::Execute(ctx, [this] (jobsystem::JobArgs) {
            RenderPath* renderPath = GetRenderPath();
            renderPath->Load();

            ActivePath(renderPath);
        });

        // Rebuild swapchain and resize buffers if the client window changes size
        m_window.SetWindowResizeCallback(std::bind(&Application::RebuildSwapchain, this));
        jobsystem::Wait(ctx);

        CYB_INFO("cyb-engine initialized in {:.2f}ms ", timer.ElapsedMilliseconds());
        CYB_INFO_HR();
    }

    void Application::UpdateLoop()
    {
        do 
        {
            // Update input state and poll window events
            input::Update(m_window.GetNativeHandle());
            if (!m_window.PollEvents())
                break;

            const float dt = m_timer.RecordElapsedSeconds();

            // If the window is not active we limit the FPS to r_maxBackgroundFps.
            // If the window is minimized we skip the update and render entirely.
            if (!m_window.IsActive())
            {
                const double TargetFrameTimeMs = 1000.0 / double(r_maxBackgroundFps.GetValue());
                const auto remainingTime = std::chrono::duration<double, std::milli>(TargetFrameTimeMs - dt);
                if (remainingTime > std::chrono::duration<double, std::milli>::zero())
                    std::this_thread::sleep_for(remainingTime);

                if (m_window.IsMinimized())
                    continue;
            }

            profiler::BeginFrame();

            // Wake up the events that need to be executed on the main thread, 
            // in thread safe manner.
            eventsystem::FireEvent(eventsystem::Event_ThreadSafePoint, 0);

            Update(dt);

            Render();

            // Compose the final image and and pass it to the swapchain for display
            CommandList cmd = m_graphicsDevice->BeginCommandList();
            m_graphicsDevice->BeginRenderPass(&m_swapchain, cmd);
            Viewport viewport{};
            viewport.width = (float)m_swapchain.GetDesc().width;
            viewport.height = (float)m_swapchain.GetDesc().height;
            m_graphicsDevice->BindViewports(&viewport, 1, cmd);

            Compose(cmd);
            m_graphicsDevice->EndRenderPass(cmd);

            profiler::EndFrame(cmd);
            m_graphicsDevice->ExecuteCommandLists();
        } while (true);
    }

    void Application::Update(double dt)
    {
        CYB_PROFILE_CPU_SCOPE("Update");
        if (m_activePath != nullptr)
            m_activePath->Update(dt);
    }

    void Application::Render()
    {
        CYB_PROFILE_CPU_SCOPE("Render");
        if (m_activePath != nullptr)
            m_activePath->Render();
    }

    void Application::Compose(rhi::CommandList cmd)
    {
        CYB_PROFILE_CPU_SCOPE("Compose");
        if (m_activePath != nullptr)
            m_activePath->Compose(cmd);
    }

    void Application::RebuildSwapchain()
    {
        assert(m_graphicsDevice);

        // TODO: This should probably be handled by the editor separately
        SetFileDialogParentWindow(m_window.GetNativeHandle());

        rhi::SwapchainDesc desc{};
        if (m_activePath != nullptr)
        {
            m_activePath->SetCanvas(m_window.GetWidth(), m_window.GetHeight());
            desc.width = m_activePath->GetPhysicalWidth();
            desc.height = m_activePath->GetPhysicalHeight();
        }
        else
        {
            desc.width = m_window.GetWidth();
            desc.height = m_window.GetHeight();
        }

        rhi::GetDevice()->CreateSwapchain(&desc, m_window.GetNativeHandle(), &m_swapchain);

        m_changeVSyncEvent = eventsystem::Subscribe(eventsystem::Event_SetVSync, [this] (uint64_t userdata) {
            SwapchainDesc desc = m_swapchain.desc;
            desc.vsync = (userdata != 0);
            bool success = m_graphicsDevice->CreateSwapchain(&desc, nullptr, &m_swapchain);
            assert(success);
        });

        r_vsync.ClearCallbacks();
        r_vsync.RegisterOnChangeCallback([] (const CVar<bool>& cvar) {
            eventsystem::FireEvent(eventsystem::Event_SetVSync, cvar.GetValue() ? 1ull : 0ull);
        });
    }

    void Application::InitGraphicsDevice()
    {
        m_graphicsDevice = std::make_unique<rhi::GraphicsDevice_Vulkan>();
        rhi::GetDevice() = m_graphicsDevice.get();
    }
}