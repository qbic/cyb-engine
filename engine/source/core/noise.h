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

    enum class NoiseStrataOp
    {
        None,
        SharpSub,
        SharpAdd,
        Quantize,
        Smooth
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

    class NoiseGenerator
    {
    public:
        NoiseGenerator(uint32_t seed = 1337);
        
        void SetSeed(uint32_t seed) { m_seed = seed; }
        void SetNoiseType(NoiseType type) { m_noiseType = type; }
        void SetFrequency(float frequency) { m_frequency = frequency; }
        void SetFractalOctaves(int octaves) { m_octaves = octaves; CalculateFractalBounding(); }
        void SetStrataFunctionOp(NoiseStrataOp op) { m_strataOp = op; }
        void SetStrata(float strata) { m_strata = strata; }
        void SetCellularReturnType(CellularReturnType type) { m_cellularReturnType = type; }
        float GetNoise(float x, float y) const;

    private:
        void CalculateFractalBounding();
        float GetNoiseSingle(uint32_t seed, float x, float y) const;
        float SinglePerlin(uint32_t seed, float x, float y) const;
        float SingleCellular(uint32_t seed, float x, float y) const;

        uint32_t m_seed = 1337;
        float m_frequency = 0.01f;
        NoiseType m_noiseType = NoiseType::Perlin;
        uint32_t m_octaves = 3;
        float m_lacunarity = 2.0f;
        float m_gain = 0.5f;
        float m_fractalBounding;

        NoiseStrataOp m_strataOp = NoiseStrataOp::None;
        float m_strata = 5.0f;

        float m_cellularJitterModifier = 1.0f;
        CellularReturnType m_cellularReturnType = CellularReturnType::Distance;
    };
}
