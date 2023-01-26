#include <algorithm>
#include "core/noise.h"
#include "core/profiler.h"
#include "editor/editor.h"
#include "editor/imgui-widgets.h"
#include "editor/terrain-generator.h"

#define UPDATE_MESH_ON_ITEM_CHANGE() if (ImGui::IsItemDeactivatedAfterEdit()) { UpdateTerrainMesh(); }
#define UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE() if (ImGui::IsItemDeactivatedAfterEdit()) { UpdateBitmapsAndTextures(); UpdateTerrainMesh(); }

namespace cyb::editor
{
    float Heightmap::GetHeightAt(uint32_t x, uint32_t y) const
    {
        return image[y * desc.width + x];
    }

    float Heightmap::GetHeightAt(const XMINT2& p) const
    {
        return GetHeightAt(p.x, p.y);
    }

    std::pair<XMINT2, float> Heightmap::FindCandidate(const XMINT2 p0, const XMINT2 p1, const XMINT2 p2) const
    {
        constexpr auto edge = [](const XMINT2& a, const XMINT2& b, const XMINT2& c)
        {
            return (b.x - c.x) * (a.y - c.y) - (b.y - c.y) * (a.x - c.x);
        };

        // Triangle bounding box
        const XMINT2 bbMin = math::Min(math::Min(p0, p1), p2);
        const XMINT2 bbMax = math::Max(math::Max(p0, p1), p2);

        // Forward differencing variables
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
        const float a = static_cast<float>(edge(p0, p1, p2));
        const float z0 = GetHeightAt(p0) / a;
        const float z1 = GetHeightAt(p1) / a;
        const float z2 = GetHeightAt(p2) / a;

        // Iterate over pixels in bounding box
        float maxError = 0;
        XMINT2 maxPoint(0, 0);
        for (int32_t y = bbMin.y; y <= bbMax.y; y++)
        {
            // compute starting offset
            int32_t dx = 0;
            if (w00 < 0 && a12 != 0)
                dx = math::Max(dx, -w00 / a12);
            if (w01 < 0 && a20 != 0)
                dx = math::Max(dx, -w01 / a20);
            if (w02 < 0 && a01 != 0)
                dx = math::Max(dx, -w02 / a01);

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
                    const float dz = math::Abs(z - GetHeightAt(x, y));
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
            maxError = 0;

        return std::make_pair(maxPoint, maxError);
    }

    class HeightmapTriangulator
    {
    public:
        HeightmapTriangulator(const Heightmap* heightmap) : m_Heightmap(heightmap) {}

        void Run(const float maxError, const int maxTriangles, const int maxPoints);

        int NumPoints() const { return m_Points.size(); }
        int NumTriangles() const { return m_Queue.size(); }
        float Error() const { return m_Errors[m_Queue[0]]; }

        std::vector<XMFLOAT3> GetPoints() const;
        std::vector<XMINT3> GetTriangles() const;

    private:
        void Flush();
        void Step();
        int AddPoint(const XMINT2 point);
        int AddTriangle(
            const int a, const int b, const int c,
            const int ab, const int bc, const int ca,
            int e);
        void Legalize(const int a);
        void QueuePush(const int t);
        int QueuePop();
        int QueuePopBack();
        void QueueRemove(const int t);
        bool QueueLess(const int i, const int j) const;
        void QueueSwap(const int i, const int j);
        void QueueUp(const int j0);
        bool QueueDown(const int i0, const int n);

        const Heightmap* m_Heightmap;
        std::vector<XMINT2> m_Points;
        std::vector<int> m_Triangles;
        std::vector<int> m_Halfedges;
        std::vector<XMINT2> m_Candidates;
        std::vector<float> m_Errors;
        std::vector<int> m_QueueIndexes;
        std::vector<int> m_Queue;
        std::vector<int> m_Pending;
    };        

    void HeightmapTriangulator::Run(const float maxError, const int maxTriangles, const int maxPoints)
    {
        // add points at all four corners
        const int x0 = 0;
        const int y0 = 0;
        const int x1 = m_Heightmap->desc.width - 1;
        const int y1 = m_Heightmap->desc.height - 1;
        const int p0 = AddPoint(XMINT2(x0, y0));
        const int p1 = AddPoint(XMINT2(x1, y0));
        const int p2 = AddPoint(XMINT2(x0, y1));
        const int p3 = AddPoint(XMINT2(x1, y1));

        // add initial two triangles
        const int t0 = AddTriangle(p3, p0, p2, -1, -1, -1, -1);
        AddTriangle(p0, p3, p1, t0, -1, -1, -1);
        Flush();

        // helper function to check if triangulation is complete
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
            points.emplace_back(static_cast<float>(p.x) / static_cast<float>(m_Heightmap->desc.width), 
                m_Heightmap->GetHeightAt(p), 
                static_cast<float>(p.y) / static_cast<float>(m_Heightmap->desc.height));
        }
        return points;
    }

    std::vector<XMINT3> HeightmapTriangulator::GetTriangles() const 
    {
        std::vector<XMINT3> triangles;
        triangles.reserve(m_Queue.size());
        for (const int i : m_Queue)
        {
            triangles.emplace_back(
                m_Triangles[i * 3 + 0],
                m_Triangles[i * 3 + 1],
                m_Triangles[i * 3 + 2]);
        }
        return triangles;
    }

    void HeightmapTriangulator::Flush() 
    {
        for (const int t : m_Pending)
        {
            // rasterize triangle to find maximum pixel error
            const auto pair = m_Heightmap->FindCandidate(
                m_Points[m_Triangles[t * 3 + 0]],
                m_Points[m_Triangles[t * 3 + 1]],
                m_Points[m_Triangles[t * 3 + 2]]);
            // update metadata
            m_Candidates[t] = pair.first;
            m_Errors[t] = pair.second;
            // add triangle to priority queue
            QueuePush(t);
        }

        m_Pending.clear();
    }

    void HeightmapTriangulator::Step() 
    {
        // pop triangle with highest error from priority queue
        const int t = QueuePop();

        const int e0 = t * 3 + 0;
        const int e1 = t * 3 + 1;
        const int e2 = t * 3 + 2;

        const int p0 = m_Triangles[e0];
        const int p1 = m_Triangles[e1];
        const int p2 = m_Triangles[e2];

        const XMINT2 a = m_Points[p0];
        const XMINT2 b = m_Points[p1];
        const XMINT2 c = m_Points[p2];
        const XMINT2 p = m_Candidates[t];

        const int pn = AddPoint(p);

        const auto collinear = [](
            const XMINT2& p0, const XMINT2& p1, const XMINT2& p2)
        {
            return (p1.y - p0.y) * (p2.x - p1.x) == (p2.y - p1.y) * (p1.x - p0.x);
        };

        const auto handleCollinear = [this](const int pn, const int a)
        {
            const int a0 = a - a % 3;
            const int al = a0 + (a + 1) % 3;
            const int ar = a0 + (a + 2) % 3;
            const int p0 = m_Triangles[ar];
            const int pr = m_Triangles[a];
            const int pl = m_Triangles[al];
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
            const int p1 = m_Triangles[bl];
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

    int HeightmapTriangulator::AddPoint(const XMINT2 point)
    {
        const int i = m_Points.size();
        m_Points.push_back(point);
        return i;
    }

    int HeightmapTriangulator::AddTriangle(
        const int a, const int b, const int c,
        const int ab, const int bc, const int ca,
        int e)
    {
        if (e < 0) 
        {
            // new halfedge index
            e = m_Triangles.size();
            // add triangle vertices
            m_Triangles.push_back(a);
            m_Triangles.push_back(b);
            m_Triangles.push_back(c);
            // add triangle halfedges
            m_Halfedges.push_back(ab);
            m_Halfedges.push_back(bc);
            m_Halfedges.push_back(ca);
            // add triangle metadata
            m_Candidates.emplace_back(0, 0);
            m_Errors.push_back(0);
            m_QueueIndexes.push_back(-1);
        }
        else 
        {
            // set triangle vertices
            m_Triangles[e + 0] = a;
            m_Triangles[e + 1] = b;
            m_Triangles[e + 2] = c;
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
        m_Pending.push_back(t);

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
            const int64_t dx = a.x - p.x;
            const int64_t dy = a.y - p.y;
            const int64_t ex = b.x - p.x;
            const int64_t ey = b.y - p.y;
            const int64_t fx = c.x - p.x;
            const int64_t fy = c.y - p.y;
            const int64_t ap = dx * dx + dy * dy;
            const int64_t bp = ex * ex + ey * ey;
            const int64_t cp = fx * fx + fy * fy;
            return dx * (ey * cp - bp * fy) - dy * (ex * cp - bp * fx) + ap * (ex * fy - ey * fx) < 0;
        };

        const int b = m_Halfedges[a];
        if (b < 0)
            return;

        const int a0 = a - a % 3;
        const int b0 = b - b % 3;
        const int al = a0 + (a + 1) % 3;
        const int ar = a0 + (a + 2) % 3;
        const int bl = b0 + (b + 2) % 3;
        const int br = b0 + (b + 1) % 3;
        const int p0 = m_Triangles[ar];
        const int pr = m_Triangles[a];
        const int pl = m_Triangles[al];
        const int p1 = m_Triangles[bl];

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
        const int i = m_Queue.size();
        m_QueueIndexes[t] = i;
        m_Queue.push_back(t);
        QueueUp(i);
    }

    int HeightmapTriangulator::QueuePop()
    {
        const int n = m_Queue.size() - 1;
        QueueSwap(0, n);
        QueueDown(0, n);
        return QueuePopBack();
    }

    int HeightmapTriangulator::QueuePopBack()
    {
        const int t = m_Queue.back();
        m_Queue.pop_back();
        m_QueueIndexes[t] = -1;
        return t;
    }

    void HeightmapTriangulator::QueueRemove(const int t)
    {
        const int i = m_QueueIndexes[t];
        if (i < 0)
        {
            const auto it = std::find(m_Pending.begin(), m_Pending.end(), t);
            assert(it != m_Pending.end());
            std::swap(*it, m_Pending.back());
            m_Pending.pop_back();
            return;
        }

        const int n = m_Queue.size() - 1;
        if (n != i)
        {
            QueueSwap(i, n);
            if (!QueueDown(i, n))
                QueueUp(i);
        }
        QueuePopBack();
    }

    bool HeightmapTriangulator::QueueLess(const int i, const int j) const
    {
        return -m_Errors[m_Queue[i]] < -m_Errors[m_Queue[j]];
    }

    void HeightmapTriangulator::QueueSwap(const int i, const int j)
    {
        const int pi = m_Queue[i];
        const int pj = m_Queue[j];
        m_Queue[i] = pj;
        m_Queue[j] = pi;
        m_QueueIndexes[pi] = j;
        m_QueueIndexes[pj] = i;
    }

    void HeightmapTriangulator::QueueUp(const int j0)
    {
        int j = j0;
        while (1) {
            int i = (j - 1) / 2;
            if (i == j || !QueueLess(j, i))
                break;
            QueueSwap(i, j);
            j = i;
        }
    }

    bool HeightmapTriangulator::QueueDown(const int i0, const int n)
    {
        int i = i0;
        while (1) 
        {
            const int j1 = 2 * i + 1;
            if (j1 >= n || j1 < 0)
                break;
            const int j2 = j1 + 1;
            int j = j1;
            if (j2 < n && QueueLess(j2, j1))
                j = j2;
            if (!QueueLess(j, i))
                break;
            QueueSwap(i, j);
            i = j;
        }
        return i > i0;
    }

    void CreateHeightmapFromNoise(jobsystem::Context& ctx, const NoiseDesc& noiseDesc, const HeightmapDesc& desc, Heightmap& heightmap)
    {
        heightmap.desc = desc;
        heightmap.image.clear();
        heightmap.image.resize(static_cast<size_t>(desc.width) * static_cast<size_t>(desc.height));
        heightmap.maxN = std::numeric_limits<float>::min();
        heightmap.minN = std::numeric_limits<float>::max();
        
        jobsystem::Dispatch(ctx, desc.height, 256, [&](jobsystem::JobArgs args)
            {
                NoiseGenerator noise;
                noise.SetSeed(noiseDesc.seed);
                noise.SetNoiseType(noiseDesc.noiseType);
                noise.SetFrequency(noiseDesc.frequency);
                noise.SetFractalOctaves(noiseDesc.octaves);
                noise.SetStrataFunctionOp(noiseDesc.strataOp);
                noise.SetStrata(noiseDesc.strata);
                noise.SetCellularReturnType(noiseDesc.cellularReturnType);

                const uint32_t y = args.jobIndex;
                float maxN = std::numeric_limits<float>::min();
                float minN = std::numeric_limits<float>::max();

                for (uint32_t x = 0; x < desc.width; ++x)
                {
                    const float xs = (float)x / (float)desc.width;
                    const float ys = (float)y / (float)desc.height;
                    const size_t offset = (size_t)y * desc.width + x;

                    const float value = noise.GetNoise(xs, ys);
                    heightmap.image[offset] = value;

                    // Get min/max noise values for height normalization
                    maxN = math::Max(maxN, value);
                    minN = math::Min(minN, value);
                }

                heightmap.maxN = math::Max(heightmap.maxN.load(), maxN);
                heightmap.minN = math::Min(heightmap.minN.load(), minN);
            });
    }

    // Normalize heightmap values to [0..1] range
    // NOTE: maxN/minN needs to be correctly set before normalizing
    void NormalizeHeightmap(jobsystem::Context& ctx, Heightmap& heightmap)
    {
        jobsystem::Dispatch(ctx, heightmap.desc.height, 256, [&](jobsystem::JobArgs args)
        {
            const float scale = 1.0f / (heightmap.maxN - heightmap.minN);
            const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < heightmap.desc.width; ++x)
            {
                const size_t offset = (size_t)y * heightmap.desc.width + x;
                const float value = heightmap.image[offset];
                heightmap.image[offset] = math::Saturate((value - heightmap.minN) * scale);
            }
        });
    }

    void NormalizeAndCombibeHeightmaps(jobsystem::Context& ctx, const Heightmap& a, const Heightmap& b, Heightmap& out, HeightmapCombineType mixType, float mixValue)
    {
        out.desc = a.desc;
        out.image.clear();
        out.image.resize(static_cast<size_t>(out.desc.width) * static_cast<size_t>(out.desc.height));
        out.maxN = std::numeric_limits<float>::min();
        out.minN = std::numeric_limits<float>::max();

        jobsystem::Dispatch(ctx, out.desc.height, 256, [&](jobsystem::JobArgs args)
        {
            float maxN = std::numeric_limits<float>::min();
            float minN = std::numeric_limits<float>::max();
            const float minA = a.minN;
            const float minB = b.minN;
            const float scaleA = 1.0f / (a.maxN - minA);
            const float scaleB = 1.0f / (b.maxN - minB);
            const uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < out.desc.width; ++x)
            {
                const size_t offset = (size_t)y * out.desc.width + x;
                const float valueA = math::Saturate((a.image[offset] - minA) * scaleA);
                const float valueB = math::Saturate((b.image[offset] - minB) * scaleB);

                float value = 0.0f;
                switch (mixType)
                {
                case HeightmapCombineType::Mul:
                    value = valueA * valueB;
                    break;
                case HeightmapCombineType::Lerp:
                    value = math::Lerp(valueA, valueB, mixValue);
                    break;
                }
                
                maxN = math::Max(maxN, value);
                minN = math::Min(minN, value);
                out.image[offset] = value;
            }

            out.maxN = math::Max(out.maxN.load(), maxN);
            out.minN = math::Min(out.minN.load(), minN);
        });
    }

    void CreateColormap(jobsystem::Context& ctx, const Heightmap& height, const ImGradient& colorBand, std::vector<uint32_t>& color)
    {
        assert(height.image.size() == height.desc.width * height.desc.height);

        color.clear();
        color.resize(height.image.size());

        jobsystem::Dispatch(ctx, height.desc.height, 128, [&](jobsystem::JobArgs args)
        {
            uint32_t y = args.jobIndex;
            for (uint32_t x = 0; x < height.desc.width; ++x)
            {
                const size_t offset = (size_t)y * height.desc.height + x;
                const float h = height.image[offset];
                color[offset] = colorBand.GetColorAt(h);
            }
        });
    }

#if 0
    static void GenerateTerrainChunk(
        cyb::scene::Scene* scene,
        ecs::Entity parent_id,
        const TerrainChunk& chunk,
        const TerrainMaps& maps,
        const ecs::Entity materialID,
        scene::MeshComponent* mesh)
    {
        const float quad_size = chunk.size / (float)chunk.resolution;
        const float pos_center = -(chunk.size * 0.5f);
        const XMFLOAT3 pos_offset = XMFLOAT3(pos_center, -15.0f, pos_center);

        const uint32_t num_vertices = chunk.resolution * chunk.resolution;
        mesh->vertex_positions.resize(num_vertices);
        mesh->vertex_colors.resize(num_vertices);
        for (uint32_t y = 0; y < chunk.resolution; ++y)
        {
            for (uint32_t x = 0; x < chunk.resolution; ++x)
            {
                const uint32_t vertex_index = y * chunk.resolution + x;
                const uint32_t image_index = (chunk.image_offset.y + y) * chunk.image_pitch + (chunk.image_offset.x + x);
                const float height = math::Lerp(chunk.minAltitude, chunk.maxAltitude, maps.height.data[image_index]);
                const XMFLOAT3 pos = XMFLOAT3(x * quad_size, height, y * quad_size);
                mesh->vertex_positions[vertex_index] = XMFLOAT3(pos_offset.x + pos.x, pos_offset.y + pos.y, pos_offset.z + pos.z);
                mesh->vertex_colors[vertex_index] = maps.color[image_index];
            }
        }
    }

    void GenerateTerrainGeometry(
        cyb::scene::Scene* scene,
        ecs::Entity parent_id,
        const TerrainMeshDescription& terrain,
        const TerrainMaps& maps,
        const ecs::Entity material_id)
    {
        Timer timer;
        timer.Record();

        const uint32_t chunk_resolution = terrain.mapResolution / terrain.numChunks;
        const uint32_t quadsPerChunkRow = chunk_resolution - 1;
        const uint32_t chunkIndexCount = (uint32_t)std::pow(quadsPerChunkRow, 2) * 6;

        // Create chunk indexes:
        std::vector<uint32_t> indices;
        indices.reserve(chunkIndexCount);
        for (uint32_t z = 0; z < quadsPerChunkRow; ++z)
        {
            for (uint32_t x = 0; x < quadsPerChunkRow; ++x)
            {
                uint32_t topLeft = z * chunk_resolution + x;
                uint32_t topRight = z * chunk_resolution + (x + 1);
                uint32_t bottomLeft = (z + 1) * chunk_resolution + x;
                uint32_t bottomRight = (z + 1) * chunk_resolution + (x + 1);

                indices.push_back(topLeft);
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
                indices.push_back(bottomRight);
                indices.push_back(bottomLeft);
            }
        }

        // Create chunk geometry:
        jobsystem::Context ctx;
        for (uint32_t z = 0; z < terrain.numChunks; ++z)
        {
            for (uint32_t x = 0; x < terrain.numChunks; ++x)
            {
                std::string chunk_name = fmt::format("Chunk_{0}_{1}", z, x);
                ecs::Entity chunk_mesh_id = scene->CreateMesh(chunk_name + "_Mesh");

                // Create an empty mesh:
                scene::MeshComponent* chunk_mesh = scene->meshes.GetComponent(chunk_mesh_id);
                scene::MeshComponent::MeshSubset subset;
                subset.materialID = material_id;
                subset.indexOffset = 0;
                subset.indexCount = (uint32_t)indices.size();
                chunk_mesh->subsets.push_back(subset);

                // Add mesh to object the object to terrain group:
                ecs::Entity chunk_object_id = scene->CreateObject(chunk_name);
                scene::ObjectComponent* chunk_object = scene->objects.GetComponent(chunk_object_id);
                chunk_object->meshID = chunk_mesh_id;
                scene->ComponentAttach(chunk_object_id, parent_id);

                // Generate geometery and create render data:
                TerrainChunk chunk;
                chunk.size = terrain.size / terrain.numChunks;
                chunk.maxAltitude = terrain.maxAltitude;
                chunk.minAltitude = terrain.minAltitude;
                chunk.resolution = chunk_resolution;
                chunk.image_offset = XMUINT2(z * chunk_resolution, x * chunk_resolution);
                chunk.image_pitch = terrain.mapResolution;
                GenerateTerrainChunk(scene, parent_id, chunk, maps, material_id, chunk_mesh);
                chunk_mesh->indices = indices;
                chunk_mesh->CreateRenderData();
            }
        }

        jobsystem::Wait(ctx);
        CYB_TRACE("Terrain mesh generated in {0:.2f}ms", timer.ElapsedMilliseconds());
    }

    void GenerateTerrain(
        const TerrainMeshDescription* desc,
        cyb::scene::Scene* scene)
    {
        // Initialize new terrain object entities
        if (terrain_object_id == ecs::kInvalidEntity)
        {
            // Create terrain material:
            terrain_material_id = scene->CreateMaterial("Terrain_Material");
            scene::MaterialComponent* material = scene->materials.GetComponent(terrain_material_id);
            material->roughness = 0.6f;
            material->metalness = 0.0f;
            material->baseColor = XMFLOAT4(1, 1, 1, 1);
            material->SetUseVertexColors(true);

            // Create a root group node
            ecs::Entity terrain_chunks_id = scene->CreateGroup("ChunkData");
            terrain_object_id = scene->CreateGroup("Terrain");
            scene->ComponentAttach(terrain_chunks_id, terrain_object_id);
            terrain_object_id = terrain_chunks_id;
        }

        // Adjust map resolution to be evenly divisible by numChunks
        TerrainMeshDescription adjustedDesc = *desc;
        adjustedDesc.mapResolution = math::GetNextDivisible(desc->mapResolution, desc->numChunks);

        // Generate terrain textures
        TerrainMaps maps;
        GenerateTerrainBitmap(adjustedDesc, maps);

        // Generate terrain mesh
        GenerateTerrainGeometry(scene, terrain_object_id, adjustedDesc, maps, terrain_material_id);
    }
#endif

    //=============================================================
    //  TerrainGenerator GUI
    //=============================================================

    static TerrainMeshDesc terrain_generator_params;

    void SetTerrainGenerationParams(const TerrainMeshDesc* params)
    {
        if (params == nullptr)
        {
            terrain_generator_params = TerrainMeshDesc();
            return;
        }

        terrain_generator_params = *params;
    }

    static const std::unordered_map<TerrainGenerator::Map, std::string> g_mapCombo = {
        { TerrainGenerator::Map::Height,            "Height"   },
        { TerrainGenerator::Map::Moisture,          "Moisture" },
        { TerrainGenerator::Map::Color,             "Color"    }
    };

    static const std::unordered_map<NoiseStrataOp, std::string> g_strataFuncCombo = {
        { NoiseStrataOp::None,                      "None"     },
        { NoiseStrataOp::SharpSub,                  "SharpSub" },
        { NoiseStrataOp::SharpAdd,                  "SharpAdd" },
        { NoiseStrataOp::Quantize,                  "Quantize" },
        { NoiseStrataOp::Smooth,                    "Smooth"   }
    };

    static const std::unordered_map<NoiseType, std::string> g_noiseTypeCombo = {
        { NoiseType::Perlin,                        "Perlin"    },
        { NoiseType::Cellular,                      "Cellular"  }
    };

    static const std::unordered_map<HeightmapCombineType, std::string> g_mixTypeCombo = {
        { HeightmapCombineType::Mul,                "Mul"       },
        { HeightmapCombineType::Lerp,               "Lerp"      }
    };

    static const std::unordered_map<CellularReturnType, std::string> g_cellularReturnTypeCombo = {
        { CellularReturnType::CellValue,            "CellValue"     },
        { CellularReturnType::Distance,             "Distance"      },
        { CellularReturnType::Distance2,            "Distance2"     },
        { CellularReturnType::Distance2Add,         "Distance2Add"  },
        { CellularReturnType::Distance2Sub,         "Distance2Sub"  },
        { CellularReturnType::Distance2Mul,         "Distance2Mul"  },
        { CellularReturnType::Distance2Div,         "Distance2Div"  }
    };

    TerrainGenerator::TerrainGenerator()
    {
        m_biomeColorBand = {
            { ImColor(173, 137,  59), 0.000f },
            { ImColor( 78,  62,  27), 0.100f },
            { ImColor(173, 137,  59), 0.142f },
            { ImColor( 16, 109,  27), 0.185f },
            { ImColor( 29, 191,  38), 0.312f },
            { ImColor( 16, 109,  27), 0.559f },
            { ImColor( 94,  94,  94), 0.607f },
            { ImColor( 45, 154,  38), 0.798f },
            { ImColor( 59, 116,  34), 0.921f }
        };

        ResetParams();
    }

    void TerrainGenerator::DrawGui(ecs::Entity selectedEntity)
    {
        if (!m_initialized)
        {
            UpdateBitmapsAndTextures();
            UpdateTerrainMesh();
            m_initialized = true;
        }

        if (ImGui::BeginTable("TerrainGenerator", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);

            // Draw the settings column:
            ImGui::TableNextColumn();

            ImGui::TextUnformatted("Terrain Mesh Settings:");
            ImGui::DragFloat("Map Size", &m_meshDesc.size, 1.0f, 1.0f, 10000.0f, "%.2fm");
            ImGui::DragFloat("Min Altitude", &m_meshDesc.minAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
            UPDATE_MESH_ON_ITEM_CHANGE();
            ImGui::DragFloat("Max Altitude", &m_meshDesc.maxAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
            UPDATE_MESH_ON_ITEM_CHANGE();
            ImGui::DragInt("Resolution", (int*)&m_meshDesc.mapResolution, 1.0f, 1, 2048), "%dpx";
            UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
            ImGui::SliderInt("NumChunks", (int*)&m_meshDesc.numChunks, 1, 32, "%d^2");
            ImGui::SliderFloat("Max Error", &m_meshDesc.maxError, 0.0001f, 0.1f, "%.4f");
            UPDATE_MESH_ON_ITEM_CHANGE();
            ImGui::Separator();

            auto editNoiseDesc = [&](const char* label_, NoiseDesc& noiseDesc)
            {
                ImGui::PushID(label_);
                ImGui::TextUnformatted(label_);
                if (gui::ComboBox("NoiseType", noiseDesc.noiseType, g_noiseTypeCombo))
                {
                    UpdateBitmapsAndTextures();
                    UpdateTerrainMesh();
                }
                ImGui::DragInt("Seed", (int*)&noiseDesc.seed, 1.0f, 0, std::numeric_limits<int>::max());
                UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
                ImGui::SliderFloat("Frequency", &noiseDesc.frequency, 0.0f, 10.0f);
                UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
                ImGui::SliderInt("FBM Octaves", (int*)&noiseDesc.octaves, 1, 8);
                UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
                if (gui::ComboBox("Strata", noiseDesc.strataOp, g_strataFuncCombo))
                {
                    UpdateBitmapsAndTextures();
                    UpdateTerrainMesh();
                }
                if (noiseDesc.strataOp != NoiseStrataOp::None)
                {
                    ImGui::SliderFloat("Amount", &noiseDesc.strata, 1.0f, 15.0f);
                    UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
                }

                if (noiseDesc.noiseType == NoiseType::Cellular)
                {
                    if (gui::ComboBox("CellularReturn", noiseDesc.cellularReturnType, g_cellularReturnTypeCombo))
                    {
                        UpdateBitmapsAndTextures();
                        UpdateTerrainMesh();
                    }
                }

                ImGui::Separator();
                ImGui::PopID();
            };

            editNoiseDesc("HeightMapNoise1 Description", m_heightmapNoise1);
            editNoiseDesc("HeightMapNoise2 Description", m_heightmapNoise2);
            ImGui::Spacing();
            if (gui::ComboBox("CombineType", m_combineType, g_mixTypeCombo))
            {
                UpdateBitmapsAndTextures();
                UpdateTerrainMesh();
            }
            if (m_combineType == HeightmapCombineType::Lerp)
            {
                ImGui::SliderFloat("HeightmapNoiseMix", &m_heightmapNoiseMix, 0.0f, 1.0f);
                UPDATE_TEXTURES_AND_MESH_ON_ITEM_CHANGE();
            }
            ImGui::Spacing();
            editNoiseDesc("MoistureMapNoise Description", m_moisturemapNoise);
            ImGui::Spacing();

           if (ImGui::GradientButton("ColorBand", &m_biomeColorBand))
               UpdateColormapAndTextures();

            ImGui::Checkbox("Use MoistureMap", &m_useMoistureMap);
            if (m_useMoistureMap)
            {
                if (ImGui::GradientButton("Moisture Colors", &m_moistureBiomeColorBand))
                {
                    UpdateColormapAndTextures();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            gui::ComboBox("Map Display", m_selectedMapType, g_mapCombo);
            ImGui::Checkbox("Draw Chunks", &m_drawChunkLines);
            ImGui::Checkbox("Draw Triangles", &m_drawTriangles);

            ImGui::Spacing();
            ImGui::Spacing();

            if (ImGui::Button("Set default params", ImVec2(-1, 0)))
            {
                ResetParams();
                UpdateBitmapsAndTextures();
                UpdateTerrainMesh();
            }

            if (ImGui::Button("Random seed", ImVec2(-1, 0)))
            {
                m_heightmapNoise1.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
                m_heightmapNoise2.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
                m_moisturemapNoise.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
                UpdateBitmapsAndTextures();
                UpdateTerrainMesh();
            }

            if (ImGui::Button("Generate terrain mesh", ImVec2(-1, 0)))
            {
                GenerateTerrainMesh();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Generated on the selected entity objects mesh. Old data is cleared");
            }

            // Display the selected terrain map image:
            ImGui::TableNextColumn();
            const graphics::Texture* tex = GetTerrainMapTexture(m_selectedMapType);

            // Draw a 1px transparent height bar to force an initial column width
            const int defaultColumnWidth = 660;
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImGui::InvisibleButton("##ForceDefaultWidth", ImVec2(defaultColumnWidth, 1));
            DrawList->AddRectFilled(ImGui::GetCursorScreenPos(),
                ImVec2(ImGui::GetCursorScreenPos().x + defaultColumnWidth, 1),
                IM_COL32(100, 200, 100, 0));

            if (tex->IsValid())
            {
                ImVec2 size = ImGui::CalcItemSize(ImVec2(-1, -2), 300, 300);
                size.x = math::Min(size.x, size.y);
                size.y = math::Min(size.x, size.y);

                ImVec2 drawPosStart = ImGui::GetCursorScreenPos();
                ImGui::Image((ImTextureID)tex, size);

                if (m_drawTriangles)
                    DrawMeshTriangles(drawPosStart);
                if (m_drawChunkLines)
                    DrawChunkLines(drawPosStart);
            }

            ImGui::EndTable();
        }
    }

    const graphics::Texture* TerrainGenerator::GetTerrainMapTexture(Map type) const
    {
        switch (m_selectedMapType)
        {
        case Map::Height: return &m_heightmapTex;;
        case Map::Moisture: return &m_moisturemapTex;
        case Map::Color: return &m_colormapTex;
        default: assert(0);
        }

        return nullptr;
    }

    void TerrainGenerator::ResetParams()
    {
        m_heightmapDesc = HeightmapDesc();

        m_heightmapNoise1 = NoiseDesc();
        m_heightmapNoise1.noiseType = NoiseType::Perlin;
        m_heightmapNoise1.frequency = 2.847;
        m_heightmapNoise1.octaves = 4;

        m_heightmapNoise2.noiseType = NoiseType::Cellular;
        m_heightmapNoise2.frequency = 0.765f;
        m_heightmapNoise2.octaves = 6;
        m_heightmapNoise2.cellularReturnType = CellularReturnType::Distance;
        m_moisturemapNoise = NoiseDesc();
    }

    void TerrainGenerator::DrawChunkLines(const ImVec2& drawStartPos) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float lineWidth = ImGui::GetCursorScreenPos().y - drawStartPos.y;
        for (uint32_t i = 1; i < m_meshDesc.numChunks; ++i)
        {
            float pos = (float)i / (float)m_meshDesc.numChunks * lineWidth;
            drawList->AddLine(ImVec2(drawStartPos.x, drawStartPos.y + pos), ImVec2(drawStartPos.x + lineWidth, drawStartPos.y + pos), IM_COL32(255, 0, 0, 255), 2.0f);
            drawList->AddLine(ImVec2(drawStartPos.x + pos, drawStartPos.y), ImVec2(drawStartPos.x + pos, drawStartPos.y + lineWidth), IM_COL32(255, 0, 0, 255), 2.0f);
        }
    }

    void TerrainGenerator::DrawMeshTriangles(const ImVec2& drawStartPos) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float imageDestWidth = static_cast<float>(ImGui::GetCursorScreenPos().y - drawStartPos.y);
        const float imageSourceWidth = static_cast<float>(m_heightmapDesc.width);
        const float scale = imageDestWidth;

        for (const auto& triangle : m_mesh.triangles)
        {
            const XMFLOAT3& p0 = m_mesh.points[triangle.x];
            const XMFLOAT3& p1 = m_mesh.points[triangle.y];
            const XMFLOAT3& p2 = m_mesh.points[triangle.z];
            drawList->AddTriangle(
                ImVec2(drawStartPos.x + p0.x * scale, drawStartPos.y + p0.z * scale),
                ImVec2(drawStartPos.x + p2.x * scale, drawStartPos.y + p2.z * scale),
                ImVec2(drawStartPos.x + p1.x * scale, drawStartPos.y + p1.z * scale),
                IM_COL32(0, 255, 128, 255), 2.0f);
        }
    }

    void TerrainGenerator::UpdateAllMaps()
    {
        CYB_TIMED_FUNCTION();

        m_heightmapDesc.width = m_heightmapDesc.height = m_meshDesc.mapResolution;
        m_moisturemapDesc.width = m_moisturemapDesc.height = m_meshDesc.mapResolution;

        jobsystem::Context ctx;
        Heightmap tempMap1;
        Heightmap tempMap2;
        CreateHeightmapFromNoise(ctx, m_heightmapNoise1, m_heightmapDesc, tempMap1);
        CreateHeightmapFromNoise(ctx, m_heightmapNoise2, m_heightmapDesc, tempMap2);
        CreateHeightmapFromNoise(ctx, m_moisturemapNoise, m_moisturemapDesc, m_moisturemap);
        jobsystem::Wait(ctx);
        NormalizeAndCombibeHeightmaps(ctx, tempMap1, tempMap2, m_heightmap, m_combineType, m_heightmapNoiseMix);
        NormalizeHeightmap(ctx, m_moisturemap);
        jobsystem::Wait(ctx);
        NormalizeHeightmap(ctx, m_heightmap);
        jobsystem::Wait(ctx);
        UpdateColormap(ctx);
        jobsystem::Wait(ctx);
    }

    void TerrainGenerator::UpdateColormap(jobsystem::Context& ctx)
    {
        CreateColormap(ctx, m_heightmap, m_biomeColorBand, m_colormap);
    }

    void TerrainGenerator::UpdateColormapAndTextures()
    {
        jobsystem::Context ctx;
        UpdateColormap(ctx);
        jobsystem::Wait(ctx);
        UpdateTextures();
        jobsystem::Wait(ctx);
    }

    void TerrainGenerator::UpdateTextures()
    {
        graphics::TextureDesc texDesc;
        graphics::SubresourceData subresourceData;

        texDesc.format = graphics::Format::R32_Float;
        texDesc.components.r = graphics::TextureComponentSwizzle::R;
        texDesc.components.g = graphics::TextureComponentSwizzle::R;
        texDesc.components.b = graphics::TextureComponentSwizzle::R;
        texDesc.width = m_meshDesc.mapResolution;
        texDesc.height = m_meshDesc.mapResolution;
        texDesc.bindFlags = graphics::BindFlags::ShaderResourceBit;

        // Create heightmap texture:
        subresourceData.mem = m_heightmap.image.data();
        subresourceData.rowPitch = texDesc.width * graphics::GetFormatStride(graphics::Format::R32_Float);
        renderer::GetDevice()->CreateTexture(&texDesc, &subresourceData, &m_heightmapTex);

        // Create moisturemap texture:
        subresourceData.mem = m_moisturemap.image.data();
        subresourceData.rowPitch = texDesc.width * graphics::GetFormatStride(graphics::Format::R32_Float);
        renderer::GetDevice()->CreateTexture(&texDesc, &subresourceData, &m_moisturemapTex);

        // Create colormap texture:
        texDesc.format = graphics::Format::R8G8B8A8_Unorm;
        texDesc.components.r = graphics::TextureComponentSwizzle::Identity;
        texDesc.components.g = graphics::TextureComponentSwizzle::Identity;
        texDesc.components.b = graphics::TextureComponentSwizzle::Identity;

        subresourceData.mem = m_colormap.data();
        subresourceData.rowPitch = texDesc.width * graphics::GetFormatStride(graphics::Format::R8G8B8A8_Unorm);
        renderer::GetDevice()->CreateTexture(&texDesc, &subresourceData, &m_colormapTex);
    }

    void TerrainGenerator::UpdateBitmapsAndTextures()
    {
        UpdateAllMaps();
        UpdateTextures();
    }

    void TerrainGenerator::UpdateTerrainMesh()
    {
        //CYB_TIMED_FUNCTION();
        HeightmapTriangulator triangulator(&m_heightmap);
        triangulator.Run(m_meshDesc.maxError, m_meshDesc.maxVertices, m_meshDesc.maxTriangles);
        m_mesh.points = triangulator.GetPoints();
        m_mesh.triangles = triangulator.GetTriangles();
        CYB_TRACE("Updated terrain mesh (numVertices={0}, numTriangles={1})", m_mesh.points.size(), m_mesh.triangles.size());
    }

    void TerrainGenerator::GenerateTerrainMesh()
    {
        //CYB_TIMED_FUNCTION();

        scene::Scene& scene = scene::GetScene();
        ecs::Entity objectID = scene.CreateObject("Terrain01");
        scene::ObjectComponent* object = scene.objects.GetComponent(objectID);
        object->meshID = scene.CreateMesh("Terrain_Mesh");
        scene::MeshComponent* mesh = scene.meshes.GetComponent(object->meshID);

        const XMFLOAT3 positionToCenter = XMFLOAT3(-(m_meshDesc.size * 0.5f), 0.0f, -(m_meshDesc.size * 0.5f));
        for (const auto& p : m_mesh.points)
        {
            mesh->vertex_positions.push_back(XMFLOAT3(
                positionToCenter.x + p.x * m_meshDesc.size,
                positionToCenter.y + math::Lerp(m_meshDesc.minAltitude, m_meshDesc.maxAltitude, p.y),
                positionToCenter.z + p.z * m_meshDesc.size));
            mesh->vertex_colors.push_back(m_biomeColorBand.GetColorAt(p.y));
        }
        
        for (const auto& tri : m_mesh.triangles)
        {
            mesh->indices.push_back(tri.x);
            mesh->indices.push_back(tri.z);
            mesh->indices.push_back(tri.y);
        }

        scene::MeshComponent::MeshSubset subset;
        subset.indexOffset = 0;
        subset.indexCount = mesh->indices.size();
        subset.materialID = scene.CreateMaterial("Terrain_Material");
        mesh->subsets.push_back(subset);
        mesh->CreateRenderData();
    }
}