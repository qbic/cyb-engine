#pragma once
#include <vector>
#include "core/noise.h"

namespace cyb::editor
{
    class HeightmapTriangulator
    {
    public:
        HeightmapTriangulator(const noise2::NoiseImageDesc* imageDesc, uint32_t width, uint32_t height, const XMINT2& offset) :
            m_heightmap(imageDesc),
            m_offset(offset),
            m_width(width),
            m_height(height)
        {
        }

        void Run(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints);

        uint32_t NumPoints() const { return (uint32_t)m_points.size(); }
        uint32_t NumTriangles() const { return (uint32_t)m_queue.size(); }
        float Error() const { return m_errors[m_queue[0]]; }

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

        const noise2::NoiseImageDesc* m_heightmap;
        XMINT2 m_offset;
        uint32_t m_width;
        uint32_t m_height;
        std::vector<XMINT2> m_points;
        std::vector<uint32_t> m_triangles;      // triangle indexes
        std::vector<int> m_halfedges;
        std::vector<XMINT2> m_candidates;
        std::vector<float> m_errors;

        std::vector<size_t> m_queueIndexes;
        std::vector<uint32_t> m_queue;
        std::vector<uint32_t> m_pending;
    };
}
