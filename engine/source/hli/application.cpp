#include "core/profiler.h"
#include "graphics/graphics-device-vulkan.h"
#include "graphics/renderer.h"
#include "systems/event-system.h"
#include "systems/job-system.h"
#include "input/input.h"
#include "hli/application.h"

#include "editor/imgui-backend.h"

using namespace cyb::graphics;

namespace cyb::hli
{
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
			std::this_thread::sleep_for(std::chrono::milliseconds((int)10));
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
		graphicsDevice->SubmitCommandList();
	}

	void Application::Initialize()
	{
		Timer timer;
		CYB_INFO("Initializing cyb-engine, please wait...");

		jobsystem::Initialize();

#if 1
		// async component initialization
		jobsystem::Context ctx;
		jobsystem::Execute(ctx, [](jobsystem::JobArgs) { input::Initialize(); });
		jobsystem::Execute(ctx, [](jobsystem::JobArgs) { renderer::Initialize(); });
		jobsystem::Execute(ctx, [&](jobsystem::JobArgs) { ImGui_Impl_CybEngine_Init(window); });

		jobsystem::Wait(ctx);
#else
		// synced initialization
		input::Initialize();
		renderer::Initialize();
		ImGui_Impl_CybEngine_Init(window);
#endif

		CYB_INFO("cyb-engine initialized in {:.2f}ms", timer.ElapsedMilliseconds());
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

	void Application::Compose(graphics::CommandList cmd)
	{
		CYB_PROFILE_CPU_SCOPE("Compose");
		if (activePath != nullptr)
			activePath->Compose(cmd);
	}

	void Application::SetWindow(platform::WindowType window)
	{
		this->window = window;

		if (graphicsDevice == nullptr)
		{
			graphicsDevice = std::make_unique<graphics::GraphicsDevice_Vulkan>();
			graphics::GetDevice() = graphicsDevice.get();
			graphics::GraphicsDevice* device = graphics::GetDevice();
		}

		canvas.SetCanvas(window);

		graphics::SwapChainDesc desc = {};
		platform::WindowProperties windowProp = platform::GetWindowProperties(window);
		desc.width = canvas.GetPhysicalWidth();
		desc.height = canvas.GetPhysicalHeight();;
		graphics::GetDevice()->CreateSwapChain(&desc, window, &swapchain);

		changeVSyncEvent = eventsystem::Subscribe(eventsystem::Event_SetVSync, [this](uint64_t userdata)
		{
			SwapChainDesc desc = swapchain.desc;
			desc.vsync = (userdata != 0);
			bool success = graphicsDevice->CreateSwapChain(&desc, nullptr, &swapchain);
			assert(success);
		});
	}
}