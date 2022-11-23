#include "core/Profiler.h"
#include "graphics/API/GraphicsDevice_Vulkan.h"
#include "graphics/Renderer.h"
#include "systems/event-system.h"
#include "systems/job-system.h"
#include "input/input.h"
#include "hli/application.h"

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

		delta_time = static_cast<float>(timer.ElapsedSeconds());
		timer.Record();

		// Wake up the events that need to be executed on the main thread, in thread safe manner:
		eventsystem::FireEvent(eventsystem::kEvent_ThreadSafePoint, 0);

		// Update the game components
		// TODO: Add a fixed-time update routine
		Update(delta_time);

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
		graphics_device->SubmitCommandList();

		profiler::EndFrame();
	}

	void Application::Initialize()
	{
		// Create a new vulkan render device and set it as default
		graphics_device = std::make_unique<graphics::GraphicsDevice_Vulkan>();
		renderer::GetDevice() = graphics_device.get();
		graphics::GraphicsDevice* device = renderer::GetDevice();

		graphics::SwapChainDesc desc = {};
		XMINT2 physical_window_size = window->GetClientSize();
		desc.width = physical_window_size.x;
		desc.height = physical_window_size.y;
		renderer::GetDevice()->CreateSwapChain(&desc, window.get(), &swapchain);

		change_vsyc_event = eventsystem::Subscribe(eventsystem::kEvent_SetVSync, [this](uint64_t userdata) {
			SwapChainDesc desc = swapchain.desc;
			desc.vsync = userdata != 0;
			bool success = graphics_device->CreateSwapChain(&desc, nullptr, &swapchain);
			assert(success);
			});

		// Initialize 3d engine components
		jobsystem::Initialize();
		input::Initialize();
		renderer::Initialize();
	}

	void Application::Update(float dt)
	{
		CYB_PROFILE_FUNCTION();
		if (active_path != nullptr)
			active_path->Update(dt);
		input::Update();
	}

	void Application::Render()
	{
		CYB_PROFILE_FUNCTION();
		if (active_path != nullptr)
			active_path->Render();
	}

	void Application::Compose(graphics::CommandList cmd)
	{
		CYB_PROFILE_FUNCTION();
		if (active_path != nullptr)
			active_path->Compose(cmd);
	}

	void Application::SetWindow(std::shared_ptr<platform::Window> window)
	{
		assert(window.get());
		this->window = window;
	}
}
