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
		active_path = component;
	}

	void Application::Run()
	{
		// Do lazy-style initialization
		if (!initialized)
		{
			Initialize();
			initialized = true;
		}

		profiler::BeginFrame();
		m_deltaTime = timer.RecordElapsedSeconds();

		// Wake up the events that need to be executed on the main thread, in thread safe manner:
		eventsystem::FireEvent(eventsystem::Event_ThreadSafePoint, 0);

		// Update the game components
		// TODO: Add a fixed-time update routine
		Update(m_deltaTime);

		// Render the scene
		Render();

		// Compose the final image and and pass it to the swapchain for display
		CommandList cmd = graphics_device->BeginCommandList();
		graphics_device->BeginRenderPass(&swapchain, cmd);
		Viewport viewport;
		viewport.width = (float)swapchain.GetDesc().width;
		viewport.height = (float)swapchain.GetDesc().height;
		graphics_device->BindViewports(&viewport, 1, cmd);

		Compose(cmd);
		graphics_device->EndRenderPass(cmd);

		profiler::EndFrame(cmd);
		graphics_device->SubmitCommandList();
	}

	void Application::Initialize()
	{
		// Create a new vulkan render device and set it as default
		graphics_device = std::make_unique<graphics::GraphicsDevice_Vulkan>();
		graphics::GetDevice() = graphics_device.get();
		graphics::GraphicsDevice* device = graphics::GetDevice();

		graphics::SwapChainDesc desc = {};
		platform::WindowProperties windowProp = platform::GetWindowProperties(window);
		desc.width = windowProp.width;
		desc.height = windowProp.height;
		graphics::GetDevice()->CreateSwapChain(&desc, window, &swapchain);

		change_vsyc_event = eventsystem::Subscribe(eventsystem::Event_SetVSync, [this](uint64_t userdata) {
			SwapChainDesc desc = swapchain.desc;
			desc.vsync = (userdata != 0);
			bool success = graphics_device->CreateSwapChain(&desc, nullptr, &swapchain);
			assert(success);
			});

		ImGui_Impl_CybEngine_Init(window);

		// Initialize 3d engine components
		jobsystem::Initialize();
		input::Initialize();
		renderer::Initialize();
	}

	void Application::Update(double dt)
	{
		CYB_PROFILE_CPU_SCOPE("Update");
		if (active_path != nullptr)
			active_path->Update(dt);
		input::Update(window);
	}

	void Application::Render()
	{
		CYB_PROFILE_CPU_SCOPE("Render");
		if (active_path != nullptr)
			active_path->Render();
	}

	void Application::Compose(graphics::CommandList cmd)
	{
		CYB_PROFILE_CPU_SCOPE("Compose");
		if (active_path != nullptr)
			active_path->Compose(cmd);
	}

	void Application::SetWindow(platform::WindowType window)
	{
		this->window = window;
	}
}