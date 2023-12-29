#include "core/profiler.h"
#include "editor/heightmap.h"

namespace cyb::editor
{
    float ApplyStrata(float value, StrataOp op, float strength)
    {
        switch (op)
        {
        case StrataOp::SharpSub:
        {
            const float steps = -std::abs(std::sin(value * strength * math::PI) * (0.1f / strength * math::PI));
            value = (value * 0.5f + steps * 0.5f);
        } break;
        case StrataOp::SharpAdd:
        {
            const float steps = std::abs(std::sin(value * strength * math::PI) * (0.1f / strength * math::PI));
            value = (value * 0.5f + steps * 0.5f);

        } break;
        case StrataOp::Quantize:
        {
            const float strata = strength * 2.0f;
            value = int(value * strata) * 1.0f / strata;
        } break;
        case StrataOp::Smooth:
        {
            const float strata = strength * 2.0f;
            const float steps = std::sin(value * strata * math::PI) * (0.1f / strata * math::PI);
            value = value * 0.5f + steps * 0.5f;
        } break;
        default:
            break;
        }

        return value;
    }

    float ApplyMix(float valueA, float valueB, MixOp op, float strength)
    {
        float value = 0.0f;
        switch (op)
        {
        case MixOp::Add:
            value = valueA + valueB;
            break;
        case MixOp::Sub:
            value = valueA - valueB;
            break;
        case MixOp::Mul:
            value = valueA * valueB;
            break;
        case MixOp::Lerp:
            value = math::Lerp(valueA, valueB, strength);
            break;
        }

        return value;
    }

    void HeightmapGenerator::UnlockMinMax()
    {
        minValue = std::numeric_limits<float>::max();
        maxValue = std::numeric_limits<float>::min();
        lockedMinMax = false;
    }

    void HeightmapGenerator::LockMinMax()
    {
        lockedMinMax = true;
    }

    float HeightmapGenerator::GetValue(float x, float y) const
    {
        if (inputList.empty())
        {
            return 0;
        }

        auto calcValue = [x, y](const Input& input)
        {
            cyb::noise::Generator noise(input.noise);
            float value = noise.GetValue(x, y);
            value = ApplyStrata(value, input.strataOp, input.strata);
            value = std::pow(math::Max(0.0f, value), input.exponent);
            return value;
        };

        float valueA = calcValue(inputList[0]);
        float valueB = 0;
        for (size_t i = 0; i < inputList.size(); i++)
        {
            bool hasValueB = ((i + 1) < inputList.size());
            if (hasValueB)
            {
                valueB = calcValue(inputList[i + 1]);
                valueA = ApplyMix(valueA, valueB, inputList[i].mixOp, inputList[i].mixing);
            }
        }

        if (!lockedMinMax)
        {
            minValue = math::Min(minValue, valueA);
            maxValue = math::Max(maxValue, valueA);
        }
        else
        {
            const float scale = 1.0f / (maxValue - minValue);
            valueA = (valueA - minValue) * scale;
        }

        return valueA;
    }

    float HeightmapGenerator::GetHeightAt(const XMINT2& p) const
    {
        return GetValue(static_cast<float>(p.x) / 512.0f, static_cast<float>(p.y) / 512.0f);
    }

    std::vector<HeightmapGenerator::Input> GetDefaultInputs()
    {
        std::vector<HeightmapGenerator::Input> inputs;
        HeightmapGenerator::Input desc;

        desc.noise.type = noise::Type::Perlin;
        desc.noise.seed = 0;
        desc.noise.frequency = 0.989f;
        desc.noise.octaves = 3;
        desc.strataOp = StrataOp::Smooth;
        desc.strata = 5.0f;
        desc.exponent = 1.8f;
        desc.mixing = 0.4f;
        inputs.push_back(desc);
        
        desc.noise.type = noise::Type::Cellular;
        desc.noise.frequency = 0.56f;
        desc.noise.octaves = 4;
        desc.strataOp = StrataOp::None;
        desc.strata = 12.0f;
        desc.exponent = 1.8f;
        inputs.push_back(desc);
        
        return inputs;
    }

    HeightmapImage::HeightmapImage(int32_t width_, int32_t height_, const XMINT2& generatorOffset, const HeightmapGenerator& generator) :
        width(width_),
        height(height_)
    {
        int32_t width1 = width + 1;
        int32_t height1 = height + 1;
        data = std::make_unique<float[]>(width1 * height1);

        const float xScale = (1.0f / (float)width) * 256.0f;
        const float yScale = (1.0f / (float)height) * 256.0f;
        const float heightScale = 1.0f / (generator.maxValue - generator.minValue);

        for (int32_t y = 0; y < height1; ++y)
        {
            for (int32_t x = 0; x < width1; ++x)
            {
                const int32_t xSample = (int32_t)(std::round(x + generatorOffset.x) * xScale);
                const int32_t ySample = (int32_t)(std::round(y + generatorOffset.y) * yScale);
                const float v = generator.GetHeightAt(XMINT2(xSample, ySample));
                data[y * width1 + x] = (v - generator.minValue) * heightScale;
            }
        }
    }

    [[nodiscard]] float HeightmapImage::GetHeightAt(const XMINT2& p) const
    {
        // TODO: check for overflow
        return data[p.y * (width + 1) + p.x];
    }

    std::pair<XMINT2, float> HeightmapImage::FindCandidate(const XMINT2& p0, const XMINT2& p1, const XMINT2& p2) const
    {
        auto edge = [](const XMINT2& a, const XMINT2& b, const XMINT2& c) -> uint32_t
        {
            return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
        };

        auto computeStartingOffset = [](int32_t w, const int32_t edgeValue, const int32_t delta) -> int32_t
        {
            return (edgeValue < 0 && delta != 0) ? std::max(0, -edgeValue / delta) : 0;
        };

        // triangle bounding box
        const XMINT2 bbMin = math::Min(math::Min(p0, p1), p2);
        const XMINT2 bbMax = math::Max(math::Max(p0, p1), p2);

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

        // pre-multiplied z values at vertices
        const float triangleArea = static_cast<float>(edge(p0, p1, p2));
        const float z0 = GetHeightAt(p0) / triangleArea;
        const float z1 = GetHeightAt(p1) / triangleArea;
        const float z2 = GetHeightAt(p2) / triangleArea;

        // iterate over pixels in bounding box
        float maxError = 0;
        XMINT2 maxPoint(0, 0);
        for (int32_t y = bbMin.y; y <= bbMax.y; y++)
        {
            // compute starting offset
            int32_t dx = 0;
            dx = computeStartingOffset(dx, w00, a12);
            dx = computeStartingOffset(dx, w01, a20);
            dx = computeStartingOffset(dx, w02, a01);

            int32_t w0 = w00 + a12 * dx;
            int32_t w1 = w01 + a20 * dx;
            int32_t w2 = w02 + a01 * dx;

            bool wasInside = false;
            for (int32_t x = bbMin.x + dx; x <= bbMax.x; x++)
            {
                // check if inside triangle
                if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                {
                    wasInside = true;

                    // compute z using barycentric coordinates
                    const float z = z0 * w0 + z1 * w1 + z2 * w2;
                    const float dz = std::abs(z - GetHeightAt(XMINT2(x, y)));
                    if (dz > maxError)
                    {
                        maxError = dz;
                        maxPoint = XMINT2(x, y);
                    }
                }
                else if (wasInside)
                {
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

        if ((maxPoint.x == p0.x && maxPoint.y == p0.y) ||
            (maxPoint.x == p1.x && maxPoint.y == p1.y) ||
            (maxPoint.x == p2.x && maxPoint.y == p2.y))
        {
            maxError = 0.0f;
        }

        return std::make_pair(maxPoint, maxError);
    }

    void HeightmapTriangulator::Run(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints)
    {
        // add points at all four corners
        const int32_t x0 = 0;
        const int32_t y0 = 0;
        const int32_t x1 = m_Image->width;
        const int32_t y1 = m_Image->height;
        const int32_t p0 = AddPoint(XMINT2(x0, y0));
        const int32_t p1 = AddPoint(XMINT2(x1, y0));
        const int32_t p2 = AddPoint(XMINT2(x0, y1));
        const int32_t p3 = AddPoint(XMINT2(x1, y1));

        // add initial two triangles
        const int t0 = AddTriangle(p3, p0, p2, -1, -1, -1, -1);
        AddTriangle(p0, p3, p1, t0, -1, -1, -1);
        Flush();

        // lambda to check if triangulation is complete
        const auto done = [this, maxError, maxTriangles, maxPoints]()
        {
            return (Error() <= maxError) ||
                (maxTriangles > 0 && NumTriangles() >= maxTriangles) ||
                (maxPoints > 0 && NumPoints() >= maxPoints);
        };

        while (!done())
        {
            Step();
        }
    }

    std::vector<XMFLOAT3> HeightmapTriangulator::GetPoints() const
    {
        std::vector<XMFLOAT3> points;
        points.reserve(m_Points.size());
        for (const XMINT2& p : m_Points)
        {
            points.emplace_back(
                static_cast<float>(p.x) / static_cast<float>(m_Image->width),
                m_Image->GetHeightAt(p),
                static_cast<float>(p.y) / static_cast<float>(m_Image->height));
        }
        return points;
    }

    std::vector<XMINT3> HeightmapTriangulator::GetTriangles() const
    {
        std::vector<XMINT3> triangles;
        triangles.reserve(m_queue.size());
        for (const auto i : m_queue)
        {
            triangles.emplace_back(
                m_triangles[i * 3 + 0],
                m_triangles[i * 3 + 1],
                m_triangles[i * 3 + 2]);
        }
        return triangles;
    }

    void HeightmapTriangulator::Flush()
    {
        for (const int t : m_pending)
        {
            // rasterize triangle to find maximum pixel error
            const auto pair = m_Image->FindCandidate(
                m_Points[m_triangles[t * 3 + 0]],
                m_Points[m_triangles[t * 3 + 1]],
                m_Points[m_triangles[t * 3 + 2]]);

            // update metadata
            m_Candidates[t] = pair.first;
            m_Errors[t] = pair.second;

            // add triangle to priority queue
            QueuePush(t);
        }

        m_pending.clear();
    }

    void HeightmapTriangulator::Step()
    {
        // pop triangle with highest error from priority queue
        const int t = QueuePop();

        const int e0 = t * 3 + 0;
        const int e1 = t * 3 + 1;
        const int e2 = t * 3 + 2;

        const int p0 = m_triangles[e0];
        const int p1 = m_triangles[e1];
        const int p2 = m_triangles[e2];

        const XMINT2 a = m_Points[p0];
        const XMINT2 b = m_Points[p1];
        const XMINT2 c = m_Points[p2];
        const XMINT2 p = m_Candidates[t];

        const int pn = AddPoint(p);

        const auto collinear = [](const XMINT2& p0, const XMINT2& p1, const XMINT2& p2)
        {
            return (p1.y - p0.y) * (p2.x - p1.x) == (p2.y - p1.y) * (p1.x - p0.x);
        };

        const auto handleCollinear = [this](const int pn, const int a)
        {
            const int a0 = a - a % 3;
            const int al = a0 + (a + 1) % 3;
            const int ar = a0 + (a + 2) % 3;
            const int p0 = m_triangles[ar];
            const int pr = m_triangles[a];
            const int pl = m_triangles[al];
            const int hal = m_Halfedges[al];
            const int har = m_Halfedges[ar];

            const int b = m_Halfedges[a];

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
            const int hbl = m_Halfedges[bl];
            const int hbr = m_Halfedges[br];

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

        if (collinear(a, b, p)) {
            handleCollinear(pn, e0);
        }
        else if (collinear(b, c, p)) {
            handleCollinear(pn, e1);
        }
        else if (collinear(c, a, p)) {
            handleCollinear(pn, e2);
        }
        else {
            const int h0 = m_Halfedges[e0];
            const int h1 = m_Halfedges[e1];
            const int h2 = m_Halfedges[e2];

            const int t0 = AddTriangle(p0, p1, pn, h0, -1, -1, e0);
            const int t1 = AddTriangle(p1, p2, pn, h1, -1, t0 + 1, -1);
            const int t2 = AddTriangle(p2, p0, pn, h2, t0 + 2, t1 + 1, -1);

            Legalize(t0);
            Legalize(t1);
            Legalize(t2);
        }

        Flush();
    }

    uint32_t HeightmapTriangulator::AddPoint(const XMINT2 point)
    {
        m_Points.push_back(point);
        assert(m_Points.size() < std::numeric_limits<uint32_t>::max());
        return (uint32_t)m_Points.size() - 1;
    }

    int HeightmapTriangulator::AddTriangle(
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
            m_Halfedges.push_back(ab);
            m_Halfedges.push_back(bc);
            m_Halfedges.push_back(ca);
            // add triangle metadata
            m_Candidates.emplace_back(0, 0);
            m_Errors.push_back(0);
            m_queueIndexes.push_back(-1);
        }
        else
        {
            // set triangle vertices
            m_triangles[e + 0] = a;
            m_triangles[e + 1] = b;
            m_triangles[e + 2] = c;
            // set triangle halfedges
            m_Halfedges[e + 0] = ab;
            m_Halfedges[e + 1] = bc;
            m_Halfedges[e + 2] = ca;
        }

        // link neighboring halfedges
        if (ab >= 0)
            m_Halfedges[ab] = e + 0;
        if (bc >= 0)
            m_Halfedges[bc] = e + 1;
        if (ca >= 0)
            m_Halfedges[ca] = e + 2;

        // add triangle to pending queue for later rasterization
        const int t = e / 3;
        m_pending.push_back(t);

        // return first halfedge index
        return e;
    }

    void HeightmapTriangulator::Legalize(const int a)
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
        const auto inCircle = [](const XMINT2& a, const XMINT2& b, const XMINT2& c, const XMINT2& p)
        {
            const int32_t dx = a.x - p.x;
            const int32_t dy = a.y - p.y;
            const int32_t ex = b.x - p.x;
            const int32_t ey = b.y - p.y;
            const int32_t fx = c.x - p.x;
            const int32_t fy = c.y - p.y;
            const int64_t ap = dx * dx + dy * dy;
            const int64_t bp = ex * ex + ey * ey;
            const int64_t cp = fx * fx + fy * fy;
            return dx * (ey * cp - bp * fy) - dy * (ex * cp - bp * fx) + ap * (ex * fy - ey * fx) < 0;
        };

        const int b = m_Halfedges[a];
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

        if (!inCircle(m_Points[p0], m_Points[pr], m_Points[pl], m_Points[p1]))
            return;

        const int hal = m_Halfedges[al];
        const int har = m_Halfedges[ar];
        const int hbl = m_Halfedges[bl];
        const int hbr = m_Halfedges[br];

        QueueRemove(a / 3);
        QueueRemove(b / 3);

        const int t0 = AddTriangle(p0, p1, pl, -1, hbl, hal, a0);
        const int t1 = AddTriangle(p1, p0, pr, t0, har, hbr, b0);

        Legalize(t0 + 1);
        Legalize(t1 + 2);
    }

    void HeightmapTriangulator::QueuePush(const int t)
    {
        const uint32_t i = (uint32_t)m_queue.size();
        m_queueIndexes[t] = i;
        m_queue.push_back(t);
        QueueUp(i);
    }

    int HeightmapTriangulator::QueuePop()
    {
        const uint32_t n = (uint32_t)m_queue.size() - 1;
        QueueSwap(0, n);
        QueueDown(0, n);
        return QueuePopBack();
    }

    int HeightmapTriangulator::QueuePopBack()
    {
        const uint32_t t = m_queue.back();
        m_queue.pop_back();
        m_queueIndexes[t] = ~0;
        return t;
    }

    void HeightmapTriangulator::QueueRemove(const int t)
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

    bool HeightmapTriangulator::QueueLess(const size_t i, const size_t j) const
    {
        return -m_Errors[m_queue[i]] < -m_Errors[m_queue[j]];
    }

    void HeightmapTriangulator::QueueSwap(const size_t i, const size_t j)
    {
        const int pi = m_queue[i];
        const int pj = m_queue[j];
        m_queue[i] = pj;
        m_queue[j] = pi;
        m_queueIndexes[pi] = j;
        m_queueIndexes[pj] = i;
    }

    void HeightmapTriangulator::QueueUp(const size_t j0)
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

    bool HeightmapTriangulator::QueueDown(const size_t i0, const size_t n)
    {
        size_t i = i0;
        while (1)
        {
            const size_t j1 = 2 * i + 1;
            if (j1 >= n)
                break;
            const size_t j2 = j1 + 1;
            size_t j = j1;
            if (j2 < n&& QueueLess(j2, j1))
                j = j2;
            if (!QueueLess(j, i))
                break;
            QueueSwap(i, j);
            i = j;
        }
        return i > i0;
    }
}