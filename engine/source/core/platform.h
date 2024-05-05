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

namespace cyb {

	template <typename T, std::size_t N>
	[[nodiscard]] constexpr std::size_t CountOf(T const (&)[N]) noexcept {
		return N;
	}

#ifdef _WIN32
	using WindowHandle = HWND;
#else
	using WindowHandle = void* :
#endif // _WIN32

	struct WindowInfo {
		int width;
		int height;
		float dpi;
	};

	struct VideoMode {
		uint32_t width;
		uint32_t height;
		uint32_t displayHz;

		[[nodiscard]] bool operator==(const VideoMode& other) const {
			return other.width == width && other.height == height && other.displayHz == displayHz;
		}

		[[nodiscard]] bool operator<(const VideoMode& x) const {
			if (displayHz != x.displayHz)
				return displayHz > x.displayHz;
			if (width != x.width)
				return width > x.width;
			return height > x.height;
		}
	};

	[[nodiscard]] WindowInfo GetWindowInfo(WindowHandle window);

	// only save modes that:
	//  * have 32 pixels depth
	//  * where display frequency is the same as system desktop
	//  * where vertival resolution is higher than 720 pixels
	//  * is a fixed resolution for the display (no stretching or centerd with black bars)
	//  * where aspect (width/height) >= 1.6
	bool GetVideoModesForDisplay(std::vector<VideoMode>& modes, int32_t displayNum);
	
	void Exit(int exitCode);
	void FatalError(const std::string& text);
	
	[[nodiscard]] bool FileOpenDialog(std::string& path, const std::string& filters);
	[[nodiscard]] bool FileSaveDialog(std::string& path, const std::string& filters);
}