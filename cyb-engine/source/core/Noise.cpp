#include "core/noise.h"
#include <numeric>
#include <random>

namespace cyb
{
    static const float GRAD_X[] =
    {
        1, -1, 1, -1,
        1, -1, 1, -1,
        0, 0, 0, 0
    };

    static const float GRAD_Y[] =
    {
        1, 1, -1, -1,
        0, 0, 0, 0,
        1, -1, 1, -1
    };

    NoiseGenerator::NoiseGenerator(uint32_t seed)
    {
        SetSeed(seed);
        CalculateFractalBounding();
    }

    void NoiseGenerator::SetSeed(uint32_t seed)
    {
        m_seed = seed;

        for (uint32_t i = 0; i < 256; ++i)
        {
            m_perm[i] = i;
        }

        std::mt19937_64 rand(seed);
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t rng = (uint32_t)(rand() % (256 - i));
            uint32_t j = rng + i;
            uint32_t k = m_perm[i];
            m_perm[i] = m_perm[i + 256] = m_perm[j];
            m_perm[j] = k;
            m_perm12[i] = m_perm12[i + 256] = m_perm[i] % 12;
        }
    }

    void NoiseGenerator::CalculateFractalBounding()
    {
        float amp = m_gain;
        float ampFractal = 1.0f;
        for (uint32_t i = 1; i < m_octaves; i++)
        {
            ampFractal += amp;
            amp *= m_gain;
        }
        m_fractalBounding = 1.0f / ampFractal;
    }

    float NoiseGenerator::GetNoise(float x, float y) const
    {
        x *= m_frequency;
        y *= m_frequency;

        float value = SinglePerlinFractalFBM(x, y);
        return value;
    }

    float NoiseGenerator::SinglePerlin(uint8_t offset, float x, float y) const
    {
        int x0 = math::Floor(x);
        int y0 = math::Floor(y);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float xs, ys;
        switch (m_interp)
        {
        case Interpolation::Linear:
            xs = x - (float)x0;
            ys = y - (float)y0;
            break;
        case Interpolation::Hermite:
            xs = math::InterpHermiteFunc(x - (float)x0);
            ys = math::InterpHermiteFunc(y - (float)y0);
            break;
        case Interpolation::Quintic:
            xs = math::InterpQuinticFunc(x - (float)x0);
            ys = math::InterpQuinticFunc(y - (float)y0);
            break;
        default: assert(0);
        }

        float xd0 = x - (float)x0;
        float yd0 = y - (float)y0;
        float xd1 = xd0 - 1;
        float yd1 = yd0 - 1;

        float xf0 = math::Lerp(GradCoord2D(offset, x0, y0, xd0, yd0), GradCoord2D(offset, x1, y0, xd1, yd0), xs);
        float xf1 = math::Lerp(GradCoord2D(offset, x0, y1, xd0, yd1), GradCoord2D(offset, x1, y1, xd1, yd1), xs);

        return math::Lerp(xf0, xf1, ys);
    }

    float NoiseGenerator::SinglePerlinFractalFBM(float x, float y) const
    {
        float sum = SinglePerlin(m_perm[0], x, y);
        float amp = 1;
        uint32_t i = 0;

        while (++i < m_octaves)
        {
            x *= m_lacunarity;
            y *= m_lacunarity;

            amp *= m_gain;
            sum += SinglePerlin(m_perm[i], x, y) * amp;
        }

        return sum * m_fractalBounding;
    }

    float NoiseGenerator::GradCoord2D(uint8_t offset, int x, int y, float xd, float yd) const
    {
        uint8_t lutPos = Index2D_12(offset, x, y);

        return xd * GRAD_X[lutPos] + yd * GRAD_Y[lutPos];
    }

    uint8_t NoiseGenerator::Index2D_12(uint8_t offset, int x, int y) const
    {
        return m_perm12[(x & 0xff) + m_perm[(y & 0xff) + offset]];
    }
}