// Noise code is inspired by FastNoiseLight:
// https://github.com/Auburn/FastNoiseLite#pragma
#pragma once
#include "core/mathlib.h"

namespace cyb
{
    enum class NoiseInterpolation
    {
        Linear,
        Hermite,
        Quintic
    };

    class NoiseGenerator
    {
    public:
        NoiseGenerator(uint32_t seed = 1337);
        
        void SetSeed(uint32_t seed);
        uint32_t GetSeed() const { return m_seed; }
        void SetFrequency(float frequency) { m_frequency = frequency; }
        float GetFrequency() const { return m_frequency; }
        void SetInterp(NoiseInterpolation interp) { m_interp = interp; }
        NoiseInterpolation GetInterp() const { return m_interp; }
        void SetFractalOctaves(int octaves) { m_octaves = octaves; CalculateFractalBounding(); }
        int GetFractalOctaves() const { return m_octaves; }
        float GetNoise(float x, float y) const;

        float SinglePerlin(uint8_t offset, float x, float y) const;
        float SinglePerlinFractalFBM(float x, float y) const;

    private:
        void CalculateFractalBounding();
        inline float GradCoord2D(uint8_t offset, int x, int y, float xd, float yd) const;
        inline uint8_t Index2D_12(uint8_t offset, int x, int y) const;

        uint8_t m_perm[512];                   // Permutation table
        uint8_t m_perm12[512];
        uint32_t m_seed = 1337;
        float m_frequency = 0.01f;
        NoiseInterpolation m_interp = NoiseInterpolation::Quintic;
        uint32_t m_octaves = 3;
        float m_lacunarity = 2.0f;
        float m_gain = 0.5f;
        float m_fractalBounding;
    };
}
