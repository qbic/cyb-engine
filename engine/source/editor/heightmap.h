#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include "core/noise.h"

namespace cyb::editor
{
    enum class HeightmapCombineType
    {
        Add,
        Sub,
        Mul,
        Lerp
    };

    enum class HeightmapStrataOp
    {
        None,
        SharpSub,
        SharpAdd,
        Quantize,
        Smooth
    };

    struct HeightmapDevice
    {
        NoiseDesc noise;
        HeightmapStrataOp strataOp = HeightmapStrataOp::None;
        float strata = 5.0f;
        float blur = 0.0f;
    };

    struct HeightmapDesc
    {
        uint32_t width = 256;
        uint32_t height = 256;

        HeightmapDevice device1;
        HeightmapDevice device2;
        HeightmapCombineType combineType = HeightmapCombineType::Lerp;
        float combineStrength = 0.5f;

        int iterations = 1;
        float erosion = 0.0f;
    };

    struct Heightmap
    {
        HeightmapDesc desc;
        std::vector<float> image;               // Image data in 1ch 32-bit floating point format

        float GetHeightAt(uint32_t x, uint32_t y) const;
        float GetHeightAt(const XMINT2& p) const;
        std::pair<XMINT2, float> FindCandidate(const XMINT2 p0, const XMINT2 p1, const XMINT2 p2) const;
    };

    void CreateHeightmap(const HeightmapDesc& desc, Heightmap& heightmap);
}
