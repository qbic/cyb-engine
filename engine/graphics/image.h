#pragma once
#include "graphics/renderer.h"

namespace cyb::renderer
{
    enum STENCILMODE
    {
        STENCILMODE_DISABLED,
        STENCILMODE_EQUAL,
        STENCILMODE_LESS,
        STENCILMODE_LESSEQUAL,
        STENCILMODE_GREATER,
        STENCILMODE_GREATEREQUAL,
        STENCILMODE_NOT,
        STENCILMODE_ALLWAYS,
        STENCILMODE_COUNT
    };

    enum class ImageFlag
    {
        None = 0,
        FullscreenBit = BIT(1),
        DepthTestBit = BIT(2)
    };
    CYB_ENABLE_BITMASK_OPERATORS(ImageFlag);

    struct ImageParams
    {
        ImageFlag flags = ImageFlag::None;
        XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT2 size = XMFLOAT2(1.0f, 1.0f);
        XMFLOAT2 pivot = XMFLOAT2(0.5f, 0.5f);          // (0,0) : upperleft, (0.5,0.5) : center, (1,1) : bottomright
        float rotation = 0;
        XMFLOAT2 corners[4] = {
            XMFLOAT2(0.0f, 0.0f),
            XMFLOAT2(1.0f, 0.0f),
            XMFLOAT2(0.0f, 1.0f),
            XMFLOAT2(1.0f, 1.0f)
        };

        const XMMATRIX* customRotation = nullptr;
        const XMMATRIX* customProjection = nullptr;

        uint8_t stencilRef = 0;
        STENCILMODE stencilComp = STENCILMODE_DISABLED;

        [[nodiscard]] bool IsFullscreenEnabled() const { return HasFlag(flags, ImageFlag::FullscreenBit); }
        [[nodiscard]] bool IsDepthTestEnabled() const { return HasFlag(flags, ImageFlag::DepthTestBit); }

        void EnableFullscreen() { SetFlag(flags, ImageFlag::FullscreenBit, true); }
        void EnableDepthTest() { SetFlag(flags, ImageFlag::DepthTestBit, true); }
    };

    void Image_Initialize();

    void DrawImage(const rhi::Texture* texture, const ImageParams& params, rhi::CommandList cmd);
}