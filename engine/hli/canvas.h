#pragma once
#include "core/sys.h"

namespace cyb
{
    class Canvas
    {
    public:
        Canvas() = default;
        virtual ~Canvas() = default;

        void SetCanvas(uint32_t width_, uint32_t height_, float dpi_ = 96.0f)
        {
            width = width_;
            height = height_;
            dpi = dpi_;
        }

        void SetCanvas(const Canvas& other)
        {
            width = other.width;
            height = other.height;
            dpi = other.dpi;
        }

        void SetCanvas(WindowHandle window)
        {
            const WindowInfo prop = GetWindowInfo(window);
            SetCanvas(prop.width, prop.height, prop.dpi);
        }

        [[nodiscard]] float GetDPI() const { return dpi; }
        [[nodiscard]] uint32_t GetPhysicalWidth() const { return width; }
        [[nodiscard]] uint32_t GetPhysicalHeight() const { return height; }

    private:
        uint32_t width = 0;
        uint32_t height = 0;
        float dpi = 96.0f;
    };
}