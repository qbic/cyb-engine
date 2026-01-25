#include "systems/profiler.h"
#include "editor/heightmap.h"

namespace cyb::editor
{
    Heightmap::Heightmap(
        const noise2::NoiseImageDesc* imageDesc,
        uint32_t width,
        uint32_t height,
        XMINT2 offset) :
        m_width{ width + 1 },
        m_height{ height + 1 }
    {
        m_data.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height));

        for (uint32_t y = 0; y < m_height; ++y)
        {
            for (uint32_t x = 0; x < m_width; ++x)
            {
                const XMINT2 p{ static_cast<int>(x), static_cast<int>(y) };
                const float u = float(p.x + offset.x) / float(imageDesc->size.width);
                const float v = float(p.y + offset.y) / float(imageDesc->size.height);
                m_data[static_cast<size_t>(y) * m_width + x] = imageDesc->GetValue(u, v);
            }
        }
    }

    float Heightmap::Sample(uint32_t x, uint32_t y) const noexcept
    {
        return m_data[static_cast<size_t>(y) * m_width + static_cast<size_t>(x)];
    }

    [[nodiscard]] static std::pair<XMINT2, float> FindCandidate(
        const Heightmap& hm,
        const XMINT2& p0,
        const XMINT2& p1,
        const XMINT2& p2)
    {
        auto edge = [] (const XMINT2& a, const XMINT2& b, const XMINT2& c) -> int32_t {
            return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
        };

        auto computeOffset = [] (const int32_t edgeValue, const int32_t delta) -> int32_t {
            return (edgeValue < 0 && delta != 0) ? std::max(0, (-edgeValue + (delta - 1)) / delta) : 0;
        };

        // triangle bounding box
        const XMINT2 bbMin{ std::min({ p0.x, p1.x, p2.x }), std::min({ p0.y, p1.y, p2.y }) };
        const XMINT2 bbMax{ std::max({ p0.x, p1.x, p2.x }), std::max({ p0.y, p1.y, p2.y }) };

        // forward differencing variables
        int32_t w00 = edge(p1, p2, bbMin);
        int32_t w01 = edge(p2, p0, bbMin);
        int32_t w02 = edge(p0, p1, bbMin);
        const int32_t a01 = p1.y - p0.y;
        const int32_t b01 = p0.x - p1.x;
        const int32_t a12 = p2.y - p1.y;
        const int32_t b12 = p1.x - p2.x;
        const int32_t a20 = p0.y - p2.y;
        const int32_t b20 = p2.x - p0.x;

        // Pre-multiplied z values at vertices
        const uint32_t area = edge(p0, p1, p2);
        const float z0 = hm.Sample(p0.x, p0.y) / static_cast<float>(area);
        const float z1 = hm.Sample(p1.x, p1.y) / static_cast<float>(area);
        const float z2 = hm.Sample(p2.x, p2.y) / static_cast<float>(area);

        // Iterate over pixels in bounding box
        float bestError = 0.0f;
        XMINT2 bestPoint{ 0, 0 };
        for (int32_t y = bbMin.y; y <= bbMax.y; y++)
        {
            // compute starting offset
            int32_t dx = std::max({
                computeOffset(w00, a12),
                computeOffset(w01, a20),
                computeOffset(w02, a01) });

            int32_t w0 = w00 + a12 * dx;
            int32_t w1 = w01 + a20 * dx;
            int32_t w2 = w02 + a01 * dx;

            bool wasInside = false;
            for (int32_t x = bbMin.x + dx; x <= bbMax.x; x++)
            {
                const XMINT2 point{ x, y };

                // Check if point is inside triangle
                if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                {
                    wasInside = true;

                    // Compute height error at this point
                    const float z = z0 * (float)w0 + z1 * (float)w1 + z2 * (float)w2;
                    const float actual = hm.Sample(point.x, point.y);
                    const float err = std::abs(z - actual);
                    if (err > bestError)
                    {
                        bestError = err;
                        bestPoint = point;
                    }
                }
                else if (wasInside)
                {
                    // Move to next row if we have exited the triangle
                    break;
                }

                w0 += a12;
                w1 += a20;
                w2 += a01;
            }

            w00 += b12;
            w01 += b20;
            w02 += b01;
        }

        // Do not return a vertex as candidate (meaning no interior point found)
        if ((bestPoint.x == p0.x && bestPoint.y == p0.y) ||
            (bestPoint.x == p1.x && bestPoint.y == p1.y) ||
            (bestPoint.x == p2.x && bestPoint.y == p2.y))
        {
            bestError = 0.0f;
        }

        return { bestPoint, bestError };
    }

    void DelaunayTriangulator::Triangulate(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints)
    {
        // Create an initial quad to start triangulation from
        const int32_t x1{ static_cast<int32_t>(m_width) };
        const int32_t y1{ static_cast<int32_t>(m_height) };
        const int32_t p0 = AddPoint({ 0,  0  });
        const int32_t p1 = AddPoint({ x1, 0  });
        const int32_t p2 = AddPoint({ 0,  y1 });
        const int32_t p3 = AddPoint({ x1, y1 });
        
        const int t0 = AddTriangle(p3, p0, p2, -1, -1, -1, -1);
        AddTriangle(p0, p3, p1, t0, -1, -1, -1);
        Flush();

        // lambda to check if triangulation is complete
        const auto done = [this, maxError, maxTriangles, maxPoints] () {
            return (Error() <= maxError) ||
                (maxTriangles > 0 && NumTriangles() >= maxTriangles) ||
                (maxPoints > 0 && NumPoints() >= maxPoints);
        };

        while (!done())
        {
            // pop triangle with highest error from priority queue
            const int t = QueuePop();

            SplitTriangle(t);
        }
    }

    std::vector<XMFLOAT3> DelaunayTriangulator::GetPoints() const
    {
        std::vector<XMFLOAT3> points;
        points.reserve(m_points.size());

        const float invW = 1.0f / static_cast<float>(m_width);
        const float invH = 1.0f / static_cast<float>(m_height);

        for (const XMINT2& p : m_points)
            points.emplace_back(
                static_cast<float>(p.x) * invW,
                m_heightmap.Sample(p.x, p.y),
                static_cast<float>(p.y) * invH);

        return points;
    }

    std::vector<XMINT3> DelaunayTriangulator::GetTriangles() const
    {
        std::vector<XMINT3> triangles;
        triangles.reserve(m_queue.size());

        for (const auto i : m_queue)
            triangles.emplace_back(
                m_triangles[i * 3 + 0],
                m_triangles[i * 3 + 1],
                m_triangles[i * 3 + 2]);

        return triangles;
    }

    void DelaunayTriangulator::Flush()
    {
        for (const int t : m_pending)
        {
            // rasterize triangle to find maximum pixel error
            const auto pair = FindCandidate(
                m_heightmap,
                m_points[m_triangles[t * 3 + 0]],
                m_points[m_triangles[t * 3 + 1]],
                m_points[m_triangles[t * 3 + 2]]);

            // update metadata
            m_candidates[t] = pair.first;
            m_errors[t] = pair.second;

            // add triangle to priority queue
            QueuePush(t);
        }

        m_pending.clear();
    }

    void DelaunayTriangulator::SplitTriangle(int t)
    {
        const int e0 = t * 3 + 0;
        const int e1 = t * 3 + 1;
        const int e2 = t * 3 + 2;

        const int p0 = m_triangles[e0];
        const int p1 = m_triangles[e1];
        const int p2 = m_triangles[e2];

        const XMINT2 a = m_points[p0];
        const XMINT2 b = m_points[p1];
        const XMINT2 c = m_points[p2];
        const XMINT2 p = m_candidates[t];

        const int pn = AddPoint(p);

        const auto collinear = [] (const XMINT2& p0, const XMINT2& p1, const XMINT2& p2) {
            return (p1.y - p0.y) * (p2.x - p1.x) == (p2.y - p1.y) * (p1.x - p0.x);
        };

        const auto handleCollinear = [this] (const int pn, const int a) {
            const int a0 = a - a % 3;
            const int al = a0 + (a + 1) % 3;
            const int ar = a0 + (a + 2) % 3;
            const int p0 = m_triangles[ar];
            const int pr = m_triangles[a];
            const int pl = m_triangles[al];
            const int hal = m_halfedges[al];
            const int har = m_halfedges[ar];

            const int b = m_halfedges[a];

            if (b < 0)
            {
                const int t0 = AddTriangle(pn, p0, pr, -1, har, -1, a0);
                const int t1 = AddTriangle(p0, pn, pl, t0, -1, hal, -1);
                Legalize(t0 + 1);
                Legalize(t1 + 2);
                return;
            }

            const int b0 = b - b % 3;
            const int bl = b0 + (b + 2) % 3;
            const int br = b0 + (b + 1) % 3;
            const int p1 = m_triangles[bl];
            const int hbl = m_halfedges[bl];
            const int hbr = m_halfedges[br];

            QueueRemove(b / 3);

            const int t0 = AddTriangle(p0, pr, pn, har, -1, -1, a0);
            const int t1 = AddTriangle(pr, p1, pn, hbr, -1, t0 + 1, b0);
            const int t2 = AddTriangle(p1, pl, pn, hbl, -1, t1 + 1, -1);
            const int t3 = AddTriangle(pl, p0, pn, hal, t0 + 2, t2 + 1, -1);

            Legalize(t0);
            Legalize(t1);
            Legalize(t2);
            Legalize(t3);
        };

        if (collinear(a, b, p))
        {
            handleCollinear(pn, e0);
        }
        else if (collinear(b, c, p))
        {
            handleCollinear(pn, e1);
        }
        else if (collinear(c, a, p))
        {
            handleCollinear(pn, e2);
        }
        else
        {
            const int h0 = m_halfedges[e0];
            const int h1 = m_halfedges[e1];
            const int h2 = m_halfedges[e2];

            const int t0 = AddTriangle(p0, p1, pn, h0, -1, -1, e0);
            const int t1 = AddTriangle(p1, p2, pn, h1, -1, t0 + 1, -1);
            const int t2 = AddTriangle(p2, p0, pn, h2, t0 + 2, t1 + 1, -1);

            Legalize(t0);
            Legalize(t1);
            Legalize(t2);
        }

        Flush();
    }

    uint32_t DelaunayTriangulator::AddPoint(const XMINT2 point)
    {
        m_points.push_back(point);
        assert(m_points.size() < std::numeric_limits<uint32_t>::max());
        return (uint32_t)m_points.size() - 1;
    }

    int DelaunayTriangulator::AddTriangle(
        const int a, const int b, const int c,
        const int ab, const int bc, const int ca,
        int32_t e)
    {
        if (e < 0)
        {
            // new halfedge index
            e = (int32_t)m_triangles.size();
            // add triangle vertices
            m_triangles.push_back(a);
            m_triangles.push_back(b);
            m_triangles.push_back(c);
            // add triangle halfedges
            m_halfedges.push_back(ab);
            m_halfedges.push_back(bc);
            m_halfedges.push_back(ca);
            // add triangle metadata
            m_candidates.emplace_back(0, 0);
            m_errors.push_back(0);
            m_queueIndexes.push_back(-1);
        }
        else
        {
            // set triangle vertices
            m_triangles[e + 0] = a;
            m_triangles[e + 1] = b;
            m_triangles[e + 2] = c;
            // set triangle halfedges
            m_halfedges[e + 0] = ab;
            m_halfedges[e + 1] = bc;
            m_halfedges[e + 2] = ca;
        }

        // link neighboring halfedges
        if (ab >= 0)
            m_halfedges[ab] = e + 0;
        if (bc >= 0)
            m_halfedges[bc] = e + 1;
        if (ca >= 0)
            m_halfedges[ca] = e + 2;

        // add triangle to pending queue for later rasterization
        const int t = e / 3;
        m_pending.push_back(t);

        // return first halfedge index
        return e;
    }

    void DelaunayTriangulator::Legalize(const int a)
    {
        // if the pair of triangles doesn't satisfy the Delaunay condition
        // (p1 is inside the circumcircle of [p0, pl, pr]), flip them,
        // then do the same check/flip recursively for the new pair of triangles
        //
        //           pl                    pl
        //          /||\                  /  \
        //       al/ || \bl            al/    \a
        //        /  ||  \              /      \
        //       /  a||b  \    flip    /___ar___\
        //     p0\   ||   /p1   =>   p0\---bl---/p1
        //        \  ||  /              \      /
        //       ar\ || /br             b\    /br
        //          \||/                  \  /
        //           pr                    pr
        auto inCircle = [] (const XMINT2& a, const XMINT2& b, const XMINT2& c, const XMINT2& d) {
            
            const int32_t ax = a.x - d.x;
            const int32_t ay = a.y - d.y;
            const int32_t bx = b.x - d.x;
            const int32_t by = b.y - d.y;
            const int32_t cx = c.x - d.x;
            const int32_t cy = c.y - d.y;

            const int64_t det =
                (ax * ax + ay * ay) * (bx * cy - cx * by) -
                (bx * bx + by * by) * (ax * cy - cx * ay) +
                (cx * cx + cy * cy) * (ax * by - bx * ay);
            return det < 0;
        };

        const int b = m_halfedges[a];
        if (b == -1)
            return;

        const int a0 = a - a % 3;
        const int b0 = b - b % 3;
        const int al = a0 + (a + 1) % 3;
        const int ar = a0 + (a + 2) % 3;
        const int bl = b0 + (b + 2) % 3;
        const int br = b0 + (b + 1) % 3;
        const int p0 = m_triangles[ar];
        const int pr = m_triangles[a];
        const int pl = m_triangles[al];
        const int p1 = m_triangles[bl];

        if (!inCircle(m_points[p0], m_points[pr], m_points[pl], m_points[p1]))
            return;

        const int hal = m_halfedges[al];
        const int har = m_halfedges[ar];
        const int hbl = m_halfedges[bl];
        const int hbr = m_halfedges[br];

        QueueRemove(a / 3);
        QueueRemove(b / 3);

        const int t0 = AddTriangle(p0, p1, pl, -1, hbl, hal, a0);
        const int t1 = AddTriangle(p1, p0, pr, t0, har, hbr, b0);

        Legalize(t0 + 1);
        Legalize(t1 + 2);
    }

    void DelaunayTriangulator::QueuePush(const int t)
    {
        const uint32_t i = (uint32_t)m_queue.size();
        m_queueIndexes[t] = i;
        m_queue.push_back(t);
        QueueUp(i);
    }

    int DelaunayTriangulator::QueuePop()
    {
        const uint32_t n = (uint32_t)m_queue.size() - 1;
        QueueSwap(0, n);
        QueueDown(0, n);
        return QueuePopBack();
    }

    int DelaunayTriangulator::QueuePopBack()
    {
        const uint32_t t = m_queue.back();
        m_queue.pop_back();
        m_queueIndexes[t] = ~0;
        return t;
    }

    void DelaunayTriangulator::QueueRemove(const int t)
    {
        const size_t i = m_queueIndexes[t];
        if (i == ~0)
        {
            const auto it = std::find(m_pending.begin(), m_pending.end(), t);
            assert(it != m_pending.end());
            std::swap(*it, m_pending.back());
            m_pending.pop_back();
            return;
        }

        const size_t n = m_queue.size() - 1;
        if (n != i)
        {
            QueueSwap(i, n);
            if (!QueueDown(i, n))
                QueueUp(i);
        }
        QueuePopBack();
    }

    bool DelaunayTriangulator::QueueLess(const size_t i, const size_t j) const
    {
        return m_errors[m_queue[i]] > m_errors[m_queue[j]];
    }

    void DelaunayTriangulator::QueueSwap(const size_t i, const size_t j)
    {
        const int pi = m_queue[i];
        const int pj = m_queue[j];
        m_queue[i] = pj;
        m_queue[j] = pi;
        m_queueIndexes[pi] = j;
        m_queueIndexes[pj] = i;
    }

    void DelaunayTriangulator::QueueUp(const size_t j0)
    {
        size_t j = j0;
        while (1)
        {
            int32_t i = ((int32_t)j - 1) / 2;
            if (i == j || !QueueLess(j, i))
                break;
            QueueSwap(i, j);
            j = i;
        }
    }

    bool DelaunayTriangulator::QueueDown(const size_t i0, const size_t n)
    {
        size_t i = i0;
        while (1)
        {
            const size_t j1 = 2 * i + 1;
            if (j1 >= n)
                break;
            const size_t j2 = j1 + 1;
            size_t j = j1;
            if (j2 < n && QueueLess(j2, j1))
                j = j2;
            if (!QueueLess(j, i))
                break;
            QueueSwap(i, j);
            i = j;
        }
        return i > i0;
    }
}