#include "graphics/display.h"

namespace cyb
{
    std::vector<DisplayMode> GetFullscreenDisplayModes()
    {
        std::vector<DisplayMode> modes;

        DISPLAY_DEVICEA device{};
        device.cb = sizeof(device);
        const int displayIndex = 0;
        if (!EnumDisplayDevicesA(0, displayIndex, &device, 0))
            return modes;

        DISPLAY_DEVICEA monitor{};
        monitor.cb = sizeof(monitor);
        if (!EnumDisplayDevicesA(device.DeviceName, 0, &monitor, 0))
            return modes;

        DEVMODEA devmode{};
        devmode.dmSize = sizeof(devmode);
        DWORD systemDisplayFrequency = 60;
        if (EnumDisplaySettingsA(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode))
            systemDisplayFrequency = devmode.dmDisplayFrequency;

        for (DWORD modeNum = 0; ; modeNum++)
        {
            if (!EnumDisplaySettingsA(device.DeviceName, modeNum, &devmode))
                break;

            float aspect = (float)devmode.dmPelsWidth / (float)devmode.dmPelsHeight;
            if (devmode.dmBitsPerPel < 16 ||
                devmode.dmDisplayFrequency != systemDisplayFrequency ||
                devmode.dmPelsHeight < 720 ||
                devmode.dmDisplayFixedOutput != DMDFO_DEFAULT ||
                aspect < 1.6f)
                continue;

            DisplayMode mode{};
            mode.width = devmode.dmPelsWidth;
            mode.height = devmode.dmPelsHeight;
            mode.bitsPerPixel = devmode.dmBitsPerPel;
            mode.refreshRate = devmode.dmDisplayFrequency;

            // ensure video mode is unique before adding it to the modes list
            auto uniqueSearch = std::find_if(modes.begin(), modes.end(), [&] (const DisplayMode& it) {
                return mode.width == it.width &&
                    mode.height == it.height &&
                    mode.bitsPerPixel == it.bitsPerPixel &&
                    mode.refreshRate == it.refreshRate;
            });
            if (uniqueSearch == modes.end())
                modes.push_back(mode);
        }

        return modes;
    }

    WindowInfo GetWindowInfo(WindowHandle window)
    {
        WindowInfo info = {};
        info.dpi = (float)GetDpiForWindow(window);
        RECT rect;
        GetClientRect(window, &rect);
        info.width = int(rect.right - rect.left);
        info.height = int(rect.bottom - rect.top);

        return info;
    }
} // namespace cyb