#pragma once
#include <string>
#include <vector>
#include "core/types.h"
#include "core/mathlib.h"
#include "core/enum_flags.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define CYB_DEBUGBREAK() __debugbreak()
#endif // _WIN32

static_assert(sizeof(size_t) == 8, "Only 64bit build is possible");

namespace cyb
{
#ifdef _WIN32
    using WindowHandle = HWND;
#else
    using WindowHandle = void*;
#endif // _WIN32

    struct WindowInfo
    {
        int width;
        int height;
        float dpi;
    };

    struct VideoModeInfo
    {
        uint32_t width;
        uint32_t height;
        uint32_t bitsPerPixel;
        uint32_t displayFrequency;
    };

    [[nodiscard]] WindowInfo GetWindowInfo(WindowHandle window);

    // only save modes that:
    //  * have 32 pixels depth
    //  * where display frequency is the same as system desktop
    //  * where vertival resolution is higher than 720 pixels
    //  * is a fixed resolution for the display (no stretching or centerd with black bars)
    //  * where aspect (width/height) >= 1.6
    bool GetVideoModesForDisplay(std::vector<VideoModeInfo>& modes, int32_t displayNum);

    void Exit(int exitCode);
    void FatalError(const std::string& text);

    [[nodiscard]] bool FileOpenDialog(std::string& path, const std::string& filters);
    [[nodiscard]] bool FileSaveDialog(std::string& path, const std::string& filters);
}