// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once

namespace cyb::noise
{
    enum class Type
    {
        Perlin,
        Cellular
    };

    enum class CellularReturn
    {
        CellValue,
        Distance,
        Distance2,
        Distance2Add,
        Distance2Sub,
        Distance2Mul,
        Distance2Div
    };

    struct Parameters
    {
        Type type = Type::Perlin;
        uint32_t seed = 0;              // Noise function seed value
        float frequency = 5.5f;         // Noise function frequency
        uint32_t octaves = 4;           // Fractal Brownian Motion (FBM) octaves
        float lacunarity = 2.0f;
        float gain = 0.5f;
        CellularReturn cellularReturnType = CellularReturn::Distance;
        float cellularJitterModifier = 1.0f;
    };

    class Generator
    {
    public:
        Generator(uint32_t seed = 1337) noexcept;
        Generator(const Parameters& settings) noexcept;

        void SetSeed(uint32_t seed) noexcept { m_params.seed = seed; }
        void SetType(Type type) noexcept { m_params.type = type; }
        void SetFrequency(float frequency) noexcept { m_params.frequency = frequency; }
        void SetFractalOctaves(int octaves) noexcept { m_params.octaves = octaves; CalculateFractalBounding(); }
        void SetCellularReturn(CellularReturn type) noexcept { m_params.cellularReturnType = type; }
        [[nodiscard]] float GetNoise(float x, float y) const noexcept;

    private:
        void CalculateFractalBounding() noexcept;
        [[nodiscard]] float GetNoiseSingle(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SinglePerlin(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SingleCellular(uint32_t seed, float x, float y) const noexcept;

        Parameters m_params;
        float m_fractalBounding;
    };
}
