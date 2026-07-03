#pragma once
#include "core/mathlib.h"
#include "hli/renderpath.h"

namespace cyb::hli
{
    class RenderPath2D : public RenderPath
    {
    public:
        virtual void ResizeBuffers();

        void Load() override;
        void PreUpdate() override;
        void Update(double dt) override;
        void PostUpdate() override;
        void Render() const override;
        void Compose(rhi::CommandList cmd) const override;

        /**
         * Gets the scaled internal resolution for rendering.
         * For unscaled resolution, use GetPhysicalWidth() and GetPhysicalHeight().
         */
        [[nodiscard]] UVec2 GetInternalResolution() const;

    private:
        UVec2 currentBufferSize{};
#ifndef NO_EDITOR
        bool showEditor{ false };
#endif
    };
}
