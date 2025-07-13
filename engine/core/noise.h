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
        uint32_t seed = 0;              // noise function seed value
        float frequency = 5.5f;         // noise function frequency
        uint32_t octaves = 4;           // fractal brownian motion (FBM) octaves
        float lacunarity = 2.0f;        // frequency increase per octave
        float gain = 0.5f;              // amplification increase per octave
        CellularReturn cellularReturnType = CellularReturn::Distance;
        float cellularJitterModifier = 1.0f;
    };

    class Generator
    {
    public:
        Generator(const Parameters& params_) noexcept;

        [[nodiscard]] float GetValue(float x, float y) const noexcept;
        [[nodiscard]] const Parameters& GetParams() const noexcept { return params; }

    private:
        void CalculateFractalBounding() noexcept;
        [[nodiscard]] float GetNoiseSingle(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SinglePerlin(uint32_t seed, float x, float y) const noexcept;
        [[nodiscard]] float SingleCellular(uint32_t seed, float x, float y) const noexcept;

    private:
        Parameters params;
        float fractalBounding;
    };
}
