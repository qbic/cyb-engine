#include "core/cvar.h"
#include "graphics/device_vulkan.h"
#include "graphics/renderer.h"
#include "systems/event_system.h"
#include "systems/job_system.h"
#include "systems/profiler.h"
#include "systems/resource_manager.h"
#include "input/input.h"
#include "hli/application.h"

#include "editor/imgui_backend.h"

using namespace cyb::rhi;

namespace cyb::hli
{
    CVar<bool> r_vsync("r_vsync", true, CVarFlag::RendererBit, "Enable/Disable framebuffer swap on vertical refresh");

    void Application::ActivePath(RenderPath* component)
    {
        if (component != nullptr)
            component->SetCanvas(canvas);
        activePath = component;
    }

    void Application::Run()
    {
        // Do lazy-style initialization
        if (!initialized)
        {
            Initialize();
            initialized = true;
        }

        // disable update loops if application is not active
        if (!IsWindowActive())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return;
        }

        profiler::BeginFrame();
        deltaTime = timer.RecordElapsedSeconds();

        // Wake up the events that need to be executed on the main thread, in thread safe manner:
        eventsystem::FireEvent(eventsystem::Event_ThreadSafePoint, 0);

        if (activePath != nullptr)
            activePath->SetCanvas(canvas);

        // Update the game components
        // TODO: Add a fixed-time update routine
        Update(deltaTime);

        // Render the scene
        Render();

        // Compose the final image and and pass it to the swapchain for display
        CommandList cmd = graphicsDevice->BeginCommandList();
        graphicsDevice->BeginRenderPass(&swapchain, cmd);
        Viewport viewport;
        viewport.width = (float)swapchain.GetDesc().width;
        viewport.height = (float)swapchain.GetDesc().height;
        graphicsDevice->BindViewports(&viewport, 1, cmd);

        Compose(cmd);
        graphicsDevice->EndRenderPass(cmd);

        profiler::EndFrame(cmd);
        graphicsDevice->ExecuteCommandLists();
    }

    void Application::Initialize()
    {
        Timer timer;
#ifdef CYB_DEBUG_BUILD
        CYB_INFO("Running debug build, performance will be slow!");
#endif
        CYB_INFO_HR();
        CYB_INFO("Initializing cyb-engine (asynchronous), please wait...");

        RegisterStaticCVars();
        jobsystem::Initialize();

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

    void Application::Update(double dt)
    {
        CYB_PROFILE_CPU_SCOPE("Update");
        if (activePath != nullptr)
            activePath->Update(dt);
        input::Update(window);
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