#pragma once
#include <vector>
#include "core/noise.h"

namespace cyb::editor
{
    class Heightmap
    {
    public:
        explicit Heightmap(
            const noise2::NoiseImageDesc* imageDesc,
            int32_t width,
            int32_t height,
            XMINT2 offset = { 0, 0 });
        [[nodiscard]] float Sample(int32_t x, int32_t y) const noexcept;

    protected:
        std::vector<float> m_data{};
        int32_t m_width{ 0 };
        int32_t m_height{ 0 };
    };


    class DelaunayTriangulator
    {
    public:
        DelaunayTriangulator(const Heightmap& hm, uint32_t width, uint32_t height) :
            m_heightmap(hm),
            m_width(width),
            m_height(height)
        {
        }

        void Triangulate(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints);

        [[nodiscard]] uint32_t NumPoints() const noexcept { return (uint32_t)m_points.size(); }
        [[nodiscard]] uint32_t NumTriangles() const noexcept { return (uint32_t)m_queue.size(); }
        [[nodiscard]] float Error() const { return m_errors[m_queue[0]]; }

        [[nodiscard]] std::vector<XMFLOAT3> GetPoints() const;
        [[nodiscard]] std::vector<XMINT3> GetTriangles() const;

    private:
        void Flush();
        void SplitTriangle(int t);
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

        const Heightmap& m_heightmap;
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
