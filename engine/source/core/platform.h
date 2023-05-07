#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <assert.h>
#include "core/mathlib.h"
#include "core/enum_flags.h"

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

	enum class WindowFlags
	{
		None				= 0,
		AllowMinimizeBit	= (1 << 0),
		AllowMaximizeBit	= (1 << 1)
	};
	CYB_ENABLE_BITMASK_OPERATORS(WindowFlags);

	struct WindowDesc
	{
		std::wstring title;
		Window* parent = nullptr;
		XMFLOAT2 position = XMFLOAT2(0, 0);
		XMFLOAT2 size = XMFLOAT2(1024, 768);
		WindowFlags flags = WindowFlags::None;
	};

	using Instance = void*;

	// Initialize the platform layer
	void Initialize(Instance appInstance);

	Instance GetInstance();
	std::shared_ptr<Window> CreateNewWindow(const WindowDesc& desc);

	// The main window for the application that the renderer will use.
	// All user-created windows should use this as parent.
	inline std::shared_ptr<Window> main_window;
	void CreateMainWindow(const WindowDesc& desc);
	
	void Exit(int exit_code = 0);
	void CreateMessageWindow(const std::string& msg, const std::string& windowTitle = "Warning!");

	enum class FileDialogMode
	{
		Open,
		Save
	};

	[[nodiscard]] bool FileDialog(FileDialogMode mode, const std::string& filters, std::string& output);
}