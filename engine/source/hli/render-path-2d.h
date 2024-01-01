#pragma once
#include "hli/render-path.h"

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
        void Compose(graphics::CommandList cmd) const override;

        // TODO: Change from hardcoded internal resolution
        XMUINT2 GetInternalResolution() const
        {
            return XMUINT2(GetPhysicalWidth(), GetPhysicalHeight());
            //return XMUINT2(1920, 1080);
        }

    private:
        XMUINT2 currentBufferSize = {};
#ifndef NO_EDITOR
        bool show_editor = false;
#endif
    };
}
