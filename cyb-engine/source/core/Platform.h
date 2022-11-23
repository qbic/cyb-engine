#pragma once
#include <mutex>
#include <string>
#include <assert.h>
#include "Core/Mathlib.h"

#if CYB_DEBUG_BUILD
#ifdef _WIN32
#define CYB_DEBUGBREAK() __debugbreak()
#endif
#else
#define CYB_DEBUGBREAK()
#endif

namespace cyb::platform
{
	class Window
	{
	public:
		virtual ~Window() = default;
		virtual void* GetNativePtr() = 0;
		virtual bool IsActive() const = 0;
		virtual XMINT2 GetClientSize() const = 0;
	};

	struct WindowCreateDescription
	{
		enum
		{
			kAllowMinimize = (1 << 0),
			kAllowMaximize = (1 << 1)
		};

		std::wstring title;
		Window* parent = nullptr;
		XMFLOAT2 position = XMFLOAT2(0, 0);
		XMFLOAT2 size = XMFLOAT2(1024, 768);
		uint32_t flags = 0;
	};

	using Instance = void*;

	void Initialize(Instance appInstance);
	Instance GetInstance();
	std::shared_ptr<Window> CreateNewWindow(const WindowCreateDescription& desc);

	// The main window for the application that the renderer will use.
	// Áll user created windows should use this as parent.
	inline std::shared_ptr<Window> main_window;
	void CreateMainWindow(const WindowCreateDescription& desc);
	
	void Exit(int exit_code = 0);
	void CreateMessageWindow(const std::string& msg, const std::string& windowTitle = "Warning!");
}