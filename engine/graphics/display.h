#pragma once
#include <vector>
#include <format>
#include "core/sys.h"

namespace cyb
{
#ifdef _WIN32
    using WindowHandle = HWND;
#else
    using WindowHandle = void*;
#endif // _WIN32

    struct DisplayMode
    {
        uint32_t width;
        uint32_t height;
        uint32_t bitsPerPixel;
        uint32_t refreshRate;
    };

    struct WindowInfo
    {
        int width;
        int height;
        float dpi;
    };

    [[nodiscard]] std::vector<DisplayMode> GetFullscreenDisplayModes();

    [[nodiscard]] inline std::string DisplayModeToString(const DisplayMode& mode)
    {
        return std::format("{}x{} @ {}Hz ({} bpp)", mode.width, mode.height, mode.refreshRate, mode.bitsPerPixel);
    }

    //DEPRICATED
    [[nodiscard]] WindowInfo GetWindowInfo(WindowHandle window);


} // namespace cyb