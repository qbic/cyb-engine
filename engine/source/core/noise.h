// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once
#include "core/mathlib.h"

namespace cyb
{
    enum class NoiseType
    {
        Perlin,
        Cellular
    };

    enum class CellularReturnType
    {
        CellValue,
        Distance,
        Distance2,
        Distance2Add,
        Distance2Sub,
        Distance2Mul,
        Distance2Div
    };

    struct NoiseDesc
    {
        NoiseType noiseType = NoiseType::Perlin;
        uint32_t seed = 0;              // Noise function seed value
        float frequency = 5.5f;         // Noise function frequency
        uint32_t octaves = 4;           // Fractal Brownian Motion (FBM) octaves
        float lacunarity = 2.0f;
        float gain = 0.5f;
        CellularReturnType cellularReturnType = CellularReturnType::Distance;
        float cellularJitterModifier = 1.0f;
    };

    class NoiseGenerator
    {
    public:
        NoiseGenerator(uint32_t seed = 1337);
        NoiseGenerator(const NoiseDesc& noiseDesc);

        void SetSeed(uint32_t seed) { desc.seed = seed; }
        void SetNoiseType(NoiseType type) { desc.noiseType = type; }
        void SetFrequency(float frequency) { desc.frequency = frequency; }
        void SetFractalOctaves(int octaves) { desc.octaves = octaves; CalculateFractalBounding(); }
        void SetCellularReturnType(CellularReturnType type) { desc.cellularReturnType = type; }
        float GetNoise(float x, float y) const;

    private:
        void CalculateFractalBounding();
        float GetNoiseSingle(uint32_t seed, float x, float y) const;
        float SinglePerlin(uint32_t seed, float x, float y) const;
        float SingleCellular(uint32_t seed, float x, float y) const;

        NoiseDesc desc;
        float fractalBounding;
    };
}
