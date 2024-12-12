#pragma once
#include <string>
#include <vector>
#include "core/mathlib.h"
#include "core/enum_flags.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#ifdef CYB_DEBUG_BUILD
#define CYB_DEBUGBREAK() __debugbreak()
#endif
#endif // _WIN32

#ifndef CYB_DEBUG_BUILD
#define CYB_DEBUGBREAK()
#endif

#define BIT(n)			(1 << (n))

namespace cyb {

	template <typename T, std::size_t N>
	[[nodiscard]] constexpr std::size_t CountOf(T const (&)[N]) noexcept {
		return N;
	}

#ifdef _WIN32
	using WindowHandle = HWND;
#else
	using WindowHandle = void*;
#endif // _WIN32

	struct WindowInfo {
		int width;
		int height;
		float dpi;
	};

	struct VideoMode {
		uint32_t width;
		uint32_t height;
		uint32_t bitsPerPixel;
		uint32_t displayFrequency;

		[[nodiscard]] bool operator==(const VideoMode& rhs) const {
			return rhs.width == width && rhs.height == height && bitsPerPixel == rhs.bitsPerPixel && displayFrequency == rhs.displayFrequency;
		}

		[[nodiscard]] bool operator<(const VideoMode& rhs) const {
			if (bitsPerPixel != rhs.bitsPerPixel)
				return bitsPerPixel > rhs.bitsPerPixel;
			if (displayFrequency != rhs.displayFrequency)
				return displayFrequency > rhs.displayFrequency;
			if (width != rhs.width)
				return width > rhs.width;
			return height > rhs.height;
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