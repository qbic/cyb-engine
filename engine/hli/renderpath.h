#pragma once
#include "graphics/device.h"
#include "hli/canvas.h"

namespace cyb::hli
{
    class RenderPath : public Canvas
    {
    public:
        virtual void Load() = 0;

        // Executed beforee Update()
        virtual void PreUpdate() = 0;

        // Update once per frame
        virtual void Update(double dt) = 0;

        // Executed after Update()
        virtual void PostUpdate() = 0;

        // Render the current scene to the screen
        virtual void Render() const = 0;

        // Compose the rendered layers (for example blend the layers together as Images)
        // This will be rendered to the backbuffer
        virtual void Compose(rhi::CommandList cmd) const = 0;
    };
}