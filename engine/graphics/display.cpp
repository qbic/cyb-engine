#include <ranges>
#include <algorithm>
#include "core/logger.h"
#include "core/mathlib.h"
#include "core/sys.h"
#include "graphics/display.h"

namespace cyb
{
    constexpr float AspectRatio(uint32_t width, uint32_t height)
    {
        return float(width) / float(height);
    }

#if _WIN32
    std::vector<VideoMode> EnumerateDisplayModes(const WCHAR* deviceName)
    {
        std::vector<VideoMode> modes{};

        // Get the refresh rate and aspect ratio of the current display mode
        DEVMODEW current{};
        current.dmSize = sizeof(current);
        EnumDisplaySettingsW(deviceName, ENUM_CURRENT_SETTINGS, &current);
        const uint32_t targetFrequency = current.dmDisplayFrequency;
        float targetAspectRatio = AspectRatio(current.dmPelsWidth, current.dmPelsHeight);

        DEVMODEW devmode{};
        devmode.dmSize = sizeof(devmode);

        for (DWORD i = 0; EnumDisplaySettingsW(deviceName, i, &devmode); i++)
        {
            // Only care for 32-bit mode and at least 800p resolution and the same
            // refresh rate as the current display mode
            if (devmode.dmBitsPerPel != 32 ||
                devmode.dmPelsHeight < 800 ||
                devmode.dmDisplayFrequency != targetFrequency ||
                !ApproxEqual(AspectRatio(devmode.dmPelsWidth, devmode.dmPelsHeight), targetAspectRatio))
                continue;

            VideoMode vm{};
            vm.width = devmode.dmPelsWidth;
            vm.height = devmode.dmPelsHeight;
            vm.refreshRate = devmode.dmDisplayFrequency;

            // Filter out any duplicate modes
            auto it = std::ranges::find(modes, vm);
            if (it == modes.end())
                modes.push_back(vm);
        }

        // Sort by width, height, refresh rate
        std::ranges::sort(modes, [] (const VideoMode& a, const VideoMode& b) {
            if (a.width != b.width)
                return a.width > b.width;
            if (a.height != b.height)
                return a.height > b.height;
            return a.refreshRate > b.refreshRate;
        });

        return modes;
    }

    std::vector<VideoMode> EnumeratePrimaryDisplayVideoModes()
    {
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        const int displayIndex = 0;
        if (!EnumDisplayDevicesW(0, displayIndex, &device, 0))
            return std::vector<VideoMode>{};

        DISPLAY_DEVICEW monitor{};
        monitor.cb = sizeof(monitor);
        if (!EnumDisplayDevicesW(device.DeviceName, 0, &monitor, 0))
            return std::vector<VideoMode>{};

        auto modes = EnumerateDisplayModes(device.DeviceName);

        CYB_TRACE("Available video modes for display device {}: {}", WideToUtf8(device.DeviceName), WideToUtf8(device.DeviceString));
        for (const auto& mode : modes)
            CYB_TRACE("  {}", mode);

        return modes;
    }

    UVec2 DisplayWorkArea()
    {
        RECT wa{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        return {
            static_cast<uint32_t>(wa.right - wa.left),
            static_cast<uint32_t>(wa.bottom - wa.top),
        };
    }

    WindowInfo GetWindowInfo(WindowHandle window)
    {
        WindowInfo info{};
        info.dpi = (float)GetDpiForWindow(window);

        RECT rect;
        GetClientRect(window, &rect);
        info.width = int(rect.right - rect.left);
        info.height = int(rect.bottom - rect.top);

        return info;
    }
#else
    std::vector<VideoMode> EnumeratePrimaryDisplayVideoModes()
    {
        return std::vector<VideoMode>{};
    }

    UVec2 DisplayWorkArea()
    {
        return { 0u, 0u };
    }
#endif
} // namespace cyb