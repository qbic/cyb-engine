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

    enum class CombineOp
    {
        Add,
        Sub,
        Mul,
        Lerp
    };

    struct HeightmapGenerator
    {
        struct Input
        {
            cyb::noise::Parameters noise;
            StrataOp strataOp = StrataOp::Smooth;
            float strata = 5.0f;
            float exponent = 1.0;

            // mixing is done between current, and next
            // input (if any) in the input list
            CombineOp combineOp = CombineOp::Lerp;
            float mixing = 0;               // t value for when combineOp == CombineOp::Lerp
        };

        std::vector<Input> inputList;

        bool lockedMinMax = true;
        mutable float minHeight = 0.0f;
        mutable float maxHeight = 1.0f;
        float scale = 1.0f;

        void UnlockMinMax();
        void LockMinMax();
        [[nodiscard]] bool IsMinMaxLocked() const { return lockedMinMax; }
        [[nodiscard]] float GetValue(float x, float y) const;
        [[nodiscard]] float GetHeightAt(const XMINT2& p) const;
        [[nodiscard]] std::pair<XMINT2, float> FindCandidate(uint32_t width, uint32_t height, const XMINT2& offset, const XMINT2& p0, const XMINT2& p1, const XMINT2& p2) const;
    };

    [[nodiscard]] std::vector<HeightmapGenerator::Input> GetDefaultInputs();

    void CreateHeightmapImage(std::vector<float>& heightmap, int width, int height, const HeightmapGenerator& generator, int offsetX, int offsetY, float freqScale = 1.0f);

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
