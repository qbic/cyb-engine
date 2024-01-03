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
#endif // WIN32_LEAN_AND_MEAN
#include <SDKDDKVer.h>
#include <windows.h>
#ifdef CYB_DEBUG_BUILD
#define CYB_DEBUGBREAK() __debugbreak()
#endif // CYB_DEBUG_BUILD
#endif // _WIN32

#ifndef CYB_DEBUG_BUILD
#define CYB_DEBUGBREAK()
#endif

template <typename T, std::size_t N>
constexpr std::size_t CountOf(T const (&)[N]) noexcept
{
	return N;
}

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

	struct VideoMode
	{
		uint32_t width;
		uint32_t height;
		uint32_t displayHz;

		bool operator==(const VideoMode& other) const
		{
			return other.width == width && other.height == height && other.displayHz == displayHz;
		}
	};

	bool GetVideoModesForDisplay(int32_t displayNum, std::vector<VideoMode>& modeList);
	
	void Exit(int exitCode);
	void CreateMessageWindow(const std::string& msg, const std::string& windowTitle = "Warning!");

	enum class FileDialogMode
	{
		Open,
		Save
	};

	[[nodiscard]] bool FileDialog(FileDialogMode mode, const std::string& filters, std::string& output);
}