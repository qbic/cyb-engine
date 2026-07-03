#pragma once
#include <vector>
#include <format>
#include <string>
#include "core/sys.h"
#include "core/mathlib.h"

namespace cyb
{
#ifdef _WIN32
    using NativeWindowHandle = HWND;
#else // _WIN32
    using NativeWindowHandle = void*;
#endif // _WIN32

    struct VideoMode
    {
        uint32_t width;
        uint32_t height;
        uint32_t refreshRate;

        [[nodiscard]] bool operator==(const VideoMode&) const = default;
    };

    /** Calculate the aspect ratio of a video mode. */
    [[nodiscard]] constexpr float AspectRatio(uint32_t width, uint32_t height);

    /**
     * Enumerates the available video modes for the primary display.
     * The list is sorted by width, height, and refresh rate in descending order.
     * Only 32-bit modes with at least 800p resolution, same aspect ratio, and the
     * same refresh rate as the current display mode are included.
     */
    [[nodiscard]] std::vector<VideoMode> EnumeratePrimaryDisplayVideoModes();

    /** Returns the effective work area of the primary display. */
    [[nodiscard]] UVec2 DisplayWorkArea();
} // namespace cyb

/** std::format specialization for cyb::VideoMode. */
template <>
struct std::formatter<cyb::VideoMode>
{
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const cyb::VideoMode& mode, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}x{} @ {}Hz", mode.width, mode.height, mode.refreshRate);
    }
};
