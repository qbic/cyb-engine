#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <assert.h>
#include "core/mathlib.h"
#include "core/enum_flags.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <SDKDDKVer.h>
#include <windows.h>
#endif

#if CYB_DEBUG_BUILD
#ifdef _WIN32
#define CYB_DEBUGBREAK() __debugbreak()
#endif
#else
#define CYB_DEBUGBREAK()
#endif

namespace cyb::platform
{
#ifdef _WIN32
	using WindowType = HWND;
#else
	using WindowType = void*:
#endif // _WIN32

	struct WindowProperties
	{
		int width = 0;
		int height = 0;
		float dpi = 96;
	};

	inline WindowProperties GetWindowProperties(WindowType window)
	{
		WindowProperties prop;
#ifdef _WIN32
		prop.dpi = (float)GetDpiForWindow(window);
		RECT rect;
		GetClientRect(window, &rect);
		prop.width = int(rect.right - rect.left);
		prop.height = int(rect.bottom - rect.top);
#endif // _WIN32
		return prop;
	}

	
	
	void Exit(int exitCode);
	void CreateMessageWindow(const std::string& msg, const std::string& windowTitle = "Warning!");

	enum class FileDialogMode
	{
		Open,
		Save
	};

	[[nodiscard]] bool FileDialog(FileDialogMode mode, const std::string& filters, std::string& output);
}