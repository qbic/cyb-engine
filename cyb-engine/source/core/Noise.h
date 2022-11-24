#pragma once
#include "core/mathlib.h"

namespace cyb
{
    // Stripped version of FastNoiseLite
    // https://github.com/Auburn/FastNoiseLite
    class NoiseGenerator
    {
    public:
        enum class Interpolation
        {
            Linear,
            Hermite,
            Quintic
        };

        NoiseGenerator(uint32_t seed = 1337);

        void SetSeed(uint32_t seed);
        uint32_t GetSeed() const { return m_seed; }
        void SetFrequency(float frequency) { m_frequency = frequency; }
        float GetFrequency() const { return m_frequency; }
        void SetInterp(Interpolation interp) { m_interp = interp; }
        Interpolation GetInterp() const { return m_interp; }
        void SetFractalOctaves(int octaves) { m_octaves = octaves; CalculateFractalBounding(); }
        int GetFractalOctaves() const { return m_octaves; }
        float GetNoise(float x, float y) const;

        float SinglePerlin(uint8_t offset, float x, float y) const;
        float SinglePerlinFractalFBM(float x, float y) const;

    private:
        void CalculateFractalBounding();
        float GradCoord2D(uint8_t offset, int x, int y, float xd, float yd) const;
        uint8_t Index2D_12(uint8_t offset, int x, int y) const;

        uint32_t m_perm[512];                   // Permutation table
        uint32_t m_perm12[512];
        uint32_t m_seed = 1337;
        float m_frequency = 0.01f;
        Interpolation m_interp = Interpolation::Quintic;
        uint32_t m_octaves = 3;
        float m_lacunarity = 2.0f;
        float m_gain = 0.5f;
        float m_fractalBounding;
    };
}
