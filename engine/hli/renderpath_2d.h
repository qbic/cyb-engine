#pragma once
#include "hli/renderpath.h"

namespace cyb::hli {

    class RenderPath2D : public RenderPath {
    public:
        virtual void ResizeBuffers();

        void Load() override;
        void PreUpdate() override;
        void Update(double dt) override;
        void PostUpdate() override;
        void Render() const override;
        void Compose(graphics::CommandList cmd) const override;

        [[nodiscard]] XMUINT2 GetInternalResolution() const {
            return XMUINT2(GetPhysicalWidth(), GetPhysicalHeight());
        }

    private:
        XMUINT2 currentBufferSize = {};
#ifndef NO_EDITOR
        bool showEditor = false;
#endif
    };
}
