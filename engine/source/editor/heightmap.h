#pragma once
#include <vector>
#include "core/noise.h"

namespace cyb::editor
{
    enum class StrataOp
    {
        None,
        SharpSub,
        SharpAdd,
        Quantize,
        Smooth
    };

    enum class MixOp
    {
        Add,
        Sub,
        Mul,
        Lerp
    };

    float ApplyStrata(float value, StrataOp op, float strength);
    float ApplyMix(float valueA, float valueB, MixOp op, float strength);

    struct HeightmapGenerator
    {
        struct Input
        {
            cyb::noise::Parameters noise;
            StrataOp strataOp = StrataOp::Smooth;
            float strata = 5.0f;
            float exponent = 1.0;

            // mixing is done with 2 or more inputs
            // A=input[i] B=input[i + 1]
            MixOp mixOp = MixOp::Lerp;
            float mixing = 0;               // t value for lerp
        };

        std::vector<Input> inputList;

        bool lockedMinMax = true;
        mutable float minValue = std::numeric_limits<float>::max();
        mutable float maxValue = std::numeric_limits<float>::min();

        void UnlockMinMax();
        void LockMinMax();
        [[nodiscard]] float GetValue(float x, float y) const;
        [[nodiscard]] float GetHeightAt(const XMINT2& p) const;
        [[nodiscard]] std::pair<XMINT2, float> FindCandidate(uint32_t width, uint32_t height, const XMINT2& offset, const XMINT2& p0, const XMINT2& p1, const XMINT2& p2) const;
    };

    std::vector<HeightmapGenerator::Input> GetDefaultInputs();

    class HeightmapTriangulator
    {
    public:
        HeightmapTriangulator(const HeightmapGenerator* heightmap, uint32_t width, uint32_t height, const XMINT2& offset) :
            m_Heightmap(heightmap),
            m_Offset(offset),
            m_Width(width),
            m_Height(height)
        {
        }

        void Run(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints);

        uint32_t NumPoints() const { return (uint32_t)m_Points.size(); }
        uint32_t NumTriangles() const { return (uint32_t)m_queue.size(); }
        float Error() const { return m_Errors[m_queue[0]]; }

        std::vector<XMFLOAT3> GetPoints() const;
        std::vector<XMINT3> GetTriangles() const;

    private:
        void Flush();
        void Step();
        uint32_t AddPoint(const XMINT2 point);
        int AddTriangle(
            const int a, const int b, const int c,
            const int ab, const int bc, const int ca,
            int32_t e);
        void Legalize(const int a);
        void QueuePush(const int t);
        int QueuePop();
        int QueuePopBack();
        void QueueRemove(const int t);
        bool QueueLess(const size_t i, const size_t j) const;
        void QueueSwap(const size_t i, const size_t j);
        void QueueUp(const size_t j0);
        bool QueueDown(const size_t i0, const size_t n);

        const HeightmapGenerator* m_Heightmap;
        XMINT2 m_Offset;
        uint32_t m_Width;
        uint32_t m_Height;
        std::vector<XMINT2> m_Points;
        std::vector<uint32_t> m_triangles;      // triangle indexes
        std::vector<int> m_Halfedges;
        std::vector<XMINT2> m_Candidates;
        std::vector<float> m_Errors;

        std::vector<size_t> m_queueIndexes;
        std::vector<uint32_t> m_queue;
        std::vector<uint32_t> m_pending;
    };
}
