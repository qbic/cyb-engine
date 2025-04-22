#include "graphics/image.h"
#include "systems/event_system.h"

using namespace cyb::graphics;

namespace cyb::renderer
{
    enum DEPTH_TEST_MODE
    {
        DEPTH_TEST_OFF,
        DEPTH_TEST_ON,
        DEPTH_TEST_MODE_COUNT
    };

    static Sampler sampler;
    static Shader vertShader;
    static Shader fragShader;
    static RasterizerState rasterizerState;
    static DepthStencilState depthStencilState[STENCILMODE_COUNT][DEPTH_TEST_MODE_COUNT];
    static PipelineState psoImage[STENCILMODE_COUNT][DEPTH_TEST_MODE_COUNT];
    static Texture whiteTexture;

    void DrawImage(const Texture* texture, const ImageParams& params, CommandList cmd)
    {
        GraphicsDevice* device = graphics::GetDevice();
        device->BeginEvent("Image", cmd);

        ImageConstants image = {};

        if (params.IsFullscreenEnabled())
        {
            image.flags |= IMAGE_FULLSCREEN_BIT;
        }
        else
        {
            XMMATRIX S = XMMatrixScaling(params.size.x, params.size.y, 1.0f);
            XMMATRIX M = XMMatrixRotationZ(params.rotation);
            if (params.customRotation != nullptr)
                M = M * (*params.customRotation);
            M = M * XMMatrixTranslationFromVector(XMLoadFloat3(&params.position));
            if (params.customProjection != nullptr)
                M = M * (*params.customProjection);

            for (int i = 0; i < 4; ++i)
            {
                XMVECTOR V = XMVectorSet(params.corners[i].x - params.pivot.x, params.corners[i].y - params.pivot.y, 0.0f, 1.0f);
                V = XMVector2Transform(V, S);
                XMStoreFloat4(&image.corners[i], XMVector2Transform(V, M));
            }
        }

        if (texture == nullptr)
            texture = &whiteTexture;

        device->BindSampler(&sampler, 0, cmd);
        device->BindResource(texture, 0, cmd);
        device->BindStencilRef(params.stencilRef, cmd);
        device->BindPipelineState(&psoImage[params.stencilComp][params.IsDepthTestEnabled()], cmd);
        device->BindDynamicConstantBuffer(image, CBSLOT_IMAGE, cmd);
        device->Draw(params.IsFullscreenEnabled() ? 3 : 4, 0, cmd);
        device->EndEvent(cmd);
    }

    void Image_LoadShaders()
    {
        GraphicsDevice* device = graphics::GetDevice();

        renderer::LoadShader(ShaderStage::VS, vertShader, "image.vert");
        renderer::LoadShader(ShaderStage::FS, fragShader, "image.frag");

        PipelineStateDesc desc;
        desc.vs = &vertShader;
        desc.fs = &fragShader;
        desc.rs = &rasterizerState;
        desc.pt = PrimitiveTopology::TriangleStrip;

        for (int i = 0; i < STENCILMODE_COUNT; ++i)
        {
            for (int d = 0; d < DEPTH_TEST_MODE_COUNT; ++d)
            {
                desc.dss = &depthStencilState[i][d];
                device->CreatePipelineState(&desc, &psoImage[i][d]);
            }
        }
    }

    void Image_Initialize()
    {
        GraphicsDevice* device = graphics::GetDevice();

        {
            TextureDesc desc;
            desc.width = 4;
            desc.height = 4;
            desc.format = graphics::Format::R8G8B8A8_Unorm;
            desc.bindFlags = graphics::BindFlags::ShaderResourceBit;
            desc.mipLevels = 1;

            uint32_t textureData[4 * 4] = {};
            for (int i = 0; i < (4 * 4); ++i)
                textureData[i] = 0xffffffff;

            graphics::SubresourceData subresourceData;
            subresourceData.mem = textureData;
            subresourceData.rowPitch = 4 * 4;
            device->CreateTexture(&desc, &subresourceData, &whiteTexture);
        }

        RasterizerState rs;
        rs.polygonMode = PolygonMode::Fill;
        rs.cullMode = CullMode::None;
        rs.frontFace = FrontFace::CW;
        rasterizerState = rs;

        for (int d = 0; d < DEPTH_TEST_MODE_COUNT; ++d)
        {
            DepthStencilState dsd;
            dsd.depthWriteMask = DepthWriteMask::Zero;

            switch (d)
            {
            case DEPTH_TEST_OFF:
                dsd.depthEnable = false;
                break;
            case DEPTH_TEST_ON:
                dsd.depthEnable = true;
                dsd.depthFunc = ComparisonFunc::GreaterEqual;
            }

            dsd.stencilEnable = false;
            depthStencilState[STENCILMODE_DISABLED][d] = dsd;

            dsd.stencilEnable = true;
            dsd.stencilReadMask = 0xff;
            dsd.stencilWriteMask = 0;

            dsd.frontFace.stencilPassOp = StencilOp::Keep;
            dsd.frontFace.stencilFailOp = StencilOp::Keep;
            dsd.frontFace.stencilDepthFailOp = StencilOp::Keep;
            dsd.backFace.stencilPassOp = StencilOp::Keep;
            dsd.backFace.stencilFailOp = StencilOp::Keep;
            dsd.backFace.stencilDepthFailOp = StencilOp::Keep;

            dsd.frontFace.stencilFunc = ComparisonFunc::Equal;
            dsd.backFace.stencilFunc = ComparisonFunc::Equal;
            depthStencilState[STENCILMODE_EQUAL][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::Less;
            dsd.backFace.stencilFunc = ComparisonFunc::Less;
            depthStencilState[STENCILMODE_LESS][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::LessEqual;
            dsd.backFace.stencilFunc = ComparisonFunc::LessEqual;
            depthStencilState[STENCILMODE_LESSEQUAL][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::Greater;
            dsd.backFace.stencilFunc = ComparisonFunc::Greater;
            depthStencilState[STENCILMODE_GREATER][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::GreaterEqual;
            dsd.backFace.stencilFunc = ComparisonFunc::GreaterEqual;
            depthStencilState[STENCILMODE_GREATEREQUAL][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::NotEqual;
            dsd.backFace.stencilFunc = ComparisonFunc::NotEqual;
            depthStencilState[STENCILMODE_NOT][d] = dsd;

            dsd.frontFace.stencilFunc = ComparisonFunc::Allways;
            dsd.backFace.stencilFunc = ComparisonFunc::Allways;
            depthStencilState[STENCILMODE_ALLWAYS][d] = dsd;
        }

        SamplerDesc samplerDesc;
        samplerDesc.filter = TextureFilter::Anisotropic;
        samplerDesc.addressU = TextureAddressMode::Clamp;
        samplerDesc.addressV = TextureAddressMode::Clamp;
        samplerDesc.addressW = TextureAddressMode::Clamp;
        device->CreateSampler(&samplerDesc, &sampler);

        Image_LoadShaders();
        static eventsystem::Handle handle = eventsystem::Subscribe(eventsystem::Event_ReloadShaders, [] (uint64_t userdata) { Image_LoadShaders(); });
    }
}