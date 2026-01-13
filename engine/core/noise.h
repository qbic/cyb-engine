// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once
#include <array>

namespace cyb::noise2
{
    struct PerlinNoiseParams
    {
        int seed{ 0 };
        float frequency{ 5.0 };
        uint32_t octaves{ 4 };
        float lacunarity{ 2.0 };
        float persistence{ 0.5 };
    };

    struct CellularNoiseParams
    {
        uint32_t seed{ 0 };
        float frequency{ 5.0 };
        float jitterModifier{ 1.0 };
        uint32_t octaves{ 4 };
        float lacunarity{ 2.0 };
        float persistence{ 0.5 };
    };

    /**
     * @brief Generate single octave 2D Perlin noise.
     */
    [[nodiscard]] double PerlinNoise2D(int seed, double x, double y);

    /**
     * @brief Generate fractal brownian motion (FBM) 2D Perlin noise.
     */
    [[nodiscard]] double PerlinNoise2D_FBM(const PerlinNoiseParams& param, double x, double y);
    
    /**
     * @brief Generate single octave 2D Cellular (Worley) noise.
     */
    [[nodiscard]] double CellularNoise2D(uint32_t seed, double jitterModifier, double x, double y);
    
    /**
     * @brief Generate fractal brownian motion (FBM) 2D Cellular (Worley) noise.
     */
    [[nodiscard]] double CellularNoise2D_FBM(const CellularNoiseParams& param, double x, double y);

    struct NoiseImageDimensions
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
    };

    struct NoiseImageOffset
    {
        int32_t x{ 0 };
        int32_t y{ 0 };
    };

    class NoiseImage
    {
    public:
        struct Color
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        NoiseImage() = default;
        NoiseImage(const NoiseImageDimensions& size);
        
        uint32_t GetWidth() const { return m_size.width; }
        uint32_t GetHeight() const { return m_size.height; }
        uint32_t GetStride() const { return m_stride; }

        Color* GetPtr(uint32_t row);
        Color* GetPtr(uint32_t x, uint32_t y);
        const Color* GetConstPtr(uint32_t row) const;
        const Color* GetConstPtr(uint32_t x, uint32_t y) const;

        size_t GetMemoryUsage() const;

    private:
        NoiseImageDimensions m_size;
        uint32_t m_stride{ 0 };
        std::unique_ptr<Color[]> m_image;
    };

    struct NoiseImageDesc
    {
        NoiseImageDimensions size;
        NoiseImageOffset offset;
        double freqScale{ 1.0 };

        virtual float GetValue(float x, float y) const { return 0.0f; }
    };

    void RenderNoiseImageRows(NoiseImage& image,
                              const NoiseImageDesc* desc,
                              uint32_t rowStart, uint32_t rowCount);
    std::shared_ptr<NoiseImage> RenderNoiseImage(const NoiseImageDesc& desc);
} // cyb::noise2