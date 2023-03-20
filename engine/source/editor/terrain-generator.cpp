#include <algorithm>
#include "core/noise.h"
#include "core/profiler.h"
#include "editor/editor.h"
#include "editor/widgets.h"
#include "editor/imgui-widgets.h"

#include "editor/terrain-generator.h"
#include "editor/icons_font_awesome6.h"

#define UPDATE_HEIGHTMAP_ON_CHANGE() if (ImGui::IsItemDeactivatedAfterEdit()) { UpdateHeightmapAndTextures(); }

namespace cyb::editor
{
    class HeightmapTriangulator
    {
    public:
        HeightmapTriangulator(const Heightmap* heightmap) : m_Heightmap(heightmap) {}

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

        const Heightmap* m_Heightmap;
        std::vector<XMINT2> m_Points;
        std::vector<uint32_t> m_triangles;      // triangle indexes
        std::vector<int> m_Halfedges;
        std::vector<XMINT2> m_Candidates;
        std::vector<float> m_Errors;

        std::vector<size_t> m_queueIndexes;
        std::vector<uint32_t> m_queue;
        std::vector<uint32_t> m_pending;
    };        

    void HeightmapTriangulator::Run(const float maxError, const uint32_t maxTriangles, const uint32_t maxPoints)
    {
        // add points at all four corners
        const int32_t x0 = 0;
        const int32_t y0 = 0;
        const int32_t x1 = m_Heightmap->desc.width - 1;
        const int32_t y1 = m_Heightmap->desc.height - 1;
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
            points.emplace_back(static_cast<float>(p.x) / static_cast<float>(m_Heightmap->desc.width), 
                m_Heightmap->GetHeightAt(p), 
                static_cast<float>(p.y) / static_cast<float>(m_Heightmap->desc.height));
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
            const auto pair = m_Heightmap->FindCandidate(
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
            if (j2 < n && QueueLess(j2, j1))
                j = j2;
            if (!QueueLess(j, i))
                break;
            QueueSwap(i, j);
            i = j;
        }
        return i > i0;
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
        { TerrainGenerator::Map::InputA,            "Input A"   },
        { TerrainGenerator::Map::InputB,            "Input B"   },
        { TerrainGenerator::Map::Combined,          "Combined"  }
    };

    static const std::unordered_map<HeightmapStrataOp, std::string> g_strataFuncCombo = {
        { HeightmapStrataOp::None,                  "None"     },
        { HeightmapStrataOp::SharpSub,              "SharpSub" },
        { HeightmapStrataOp::SharpAdd,              "SharpAdd" },
        { HeightmapStrataOp::Quantize,              "Quantize" },
        { HeightmapStrataOp::Smooth,                "Smooth"   }
    };

    static const std::unordered_map<NoiseType, std::string> g_noiseTypeCombo = {
        { NoiseType::Perlin,                        "Perlin"    },
        { NoiseType::Cellular,                      "Cellular"  }
    };

    static const std::unordered_map<HeightmapCombineType, std::string> g_mixTypeCombo = {
        { HeightmapCombineType::Add,                "Add"       },
        { HeightmapCombineType::Sub,                "Sub"       },
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
            { ImColor( 25, 125,  48), 0.185f },
            { ImColor( 51, 170,  28), 0.312f },
            { ImColor( 25, 153,  40), 0.559f },
            { ImColor(106, 166,  31), 0.798f },
            { ImColor(255, 255, 255), 0.921f }
        };

        SetDefaultHeightmapValues();
    }

    void TerrainGenerator::DrawGui(ecs::Entity selectedEntity)
    {
        static const char* tableName = "SettingsTable";
        static constexpr uint32_t settingsPandelWidth = 420;
        if (!m_initialized)
        {
            UpdateHeightmapAndTextures();
            m_initialized = true;
        }

        if (ImGui::BeginTable("TerrainGenerator", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoClip);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);

            // Draw the settings column:
            ImGui::TableNextColumn();
            ImGui::BeginChild("Settings panel", ImVec2(settingsPandelWidth, 700), true);

            ui::ItemLabel("Test");
            ImGui::Button(ICON_FA_TRASH_CAN);
            ui::ItemLabel("Testssasdasdasdasddasd");
            ImGui::DragFloat("Map Size", &m_meshDesc.size, 1.0f, 1.0f, 10000.0f, "%.2fm");


            if (ImGui::CollapsingHeader("Terrain Mesh Settings"))
            {
                {
                    ui::ScopedIdGuard asd("Map Size");
                    ui::ItemLabel("Map Size");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::DragFloat("", &m_meshDesc.size, 1.0f, 1.0f, 10000.0f, "%.2fm");
                }
                ImGui::DragFloat("Min Altitude", &m_meshDesc.minAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
                ImGui::DragFloat("Max Altitude", &m_meshDesc.maxAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
                ImGui::SliderInt("NumChunks", (int*)&m_meshDesc.numChunks, 1, 32, "%d^2");
                ImGui::SliderFloat("Max Error", &m_meshDesc.maxError, 0.0001f, 0.01f, "%.4f");
            }

            if (ImGui::CollapsingHeader("Heightmap Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto editNoiseDesc = [&](const char* label_, HeightmapDevice& device, graphics::Texture& texture)
                {
                    if (ImGui::CollapsingHeader(label_, ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Indent();

#if 0
                        // Draw a preview using only the top 1/3 of the noise texture
                        ImVec2 size = ImGui::CalcItemSize(ImVec2(-1, -1), 100, 100);
                        size.x = math::Min(size.x, size.y);
                        size.y = math::Min(size.x, size.y) / 3;
                        ImVec2 drawPosStart = ImGui::GetCursorScreenPos();
                        ImGui::Image((ImTextureID)&texture, size, ImVec2(0, 0), ImVec2(1, 0.33f));
#endif
                        NoiseDesc& noiseDesc = device.noise;

                        if (gui::ComboBox("Noise Type", noiseDesc.noiseType, g_noiseTypeCombo))
                            UpdateHeightmapAndTextures();
                        ImGui::DragInt("Seed", (int*)&noiseDesc.seed, 1.0f, 0, std::numeric_limits<int>::max());
                        UPDATE_HEIGHTMAP_ON_CHANGE();
                        ImGui::SliderFloat("Frequency", &noiseDesc.frequency, 0.0f, 10.0f);
                        UPDATE_HEIGHTMAP_ON_CHANGE();

                        if (ImGui::TreeNode("Fractal"))
                        {
                            ImGui::SliderInt("Octaves", (int*)&noiseDesc.octaves, 1, 8);
                            UPDATE_HEIGHTMAP_ON_CHANGE();
                            ImGui::SliderFloat("Lacunarity", &noiseDesc.lacunarity, 0.0f, 4.0f);
                            UPDATE_HEIGHTMAP_ON_CHANGE();
                            ImGui::SliderFloat("Gain", &noiseDesc.gain, 0.0f, 4.0f);
                            UPDATE_HEIGHTMAP_ON_CHANGE();
                            ImGui::TreePop();
                        }

                        if (noiseDesc.noiseType == NoiseType::Cellular)
                        {
                            if (ImGui::TreeNode("Cellular"))
                            {
                                if (gui::ComboBox("Return", noiseDesc.cellularReturnType, g_cellularReturnTypeCombo))
                                    UpdateHeightmapAndTextures();
                                ImGui::SliderFloat("Jitter", &noiseDesc.cellularJitterModifier, 0.0f, 2.5f);
                                UPDATE_HEIGHTMAP_ON_CHANGE();

                                ImGui::TreePop();
                            }
                        }

                        if (ImGui::TreeNode("Post Filter"))
                        {
                            if (gui::ComboBox("Strata", device.strataOp, g_strataFuncCombo))
                                UpdateHeightmapAndTextures();

                            if (device.strataOp != HeightmapStrataOp::None)
                            {
                                ImGui::SliderFloat("Amount", &device.strata, 1.0f, 15.0f);
                                UPDATE_HEIGHTMAP_ON_CHANGE();
                            }

                            ImGui::SliderFloat("Blur", &device.blur, 0.0f, 10.0f);
                            UPDATE_HEIGHTMAP_ON_CHANGE();
                            ImGui::TreePop();
                        }

                        ImGui::Unindent();
                    }
                };

                if (ImGui::DragInt("Resolution", (int*)&m_heightmapDesc.width, 1.0f, 1, 2048, "%dpx"))
                    m_heightmapDesc.height = m_heightmapDesc.width;
                UPDATE_HEIGHTMAP_ON_CHANGE();

                editNoiseDesc("Heightmap Input A", m_heightmapDesc.device1, m_heightmapInputATex);
                editNoiseDesc("Heightmap Input B", m_heightmapDesc.device2, m_heightmapInputBTex);
                ImGui::Spacing();
                if (gui::ComboBox("Combine Type", m_heightmapDesc.combineType, g_mixTypeCombo))
                    UpdateHeightmapAndTextures();
                if (m_heightmapDesc.combineType == HeightmapCombineType::Lerp)
                {
                    ImGui::SliderFloat("Combine Strength", &m_heightmapDesc.combineStrength, 0.0f, 1.0f);
                    UPDATE_HEIGHTMAP_ON_CHANGE();
                }

                ImGui::SliderFloat("Exponent", &m_heightmapDesc.exponent, 0.0f, 4.0f);
                UPDATE_HEIGHTMAP_ON_CHANGE();
            }

            ImGui::Spacing();
            //editNoiseDesc("MoistureMapNoise Description", m_moisturemapNoise);
            //ImGui::Spacing();

           if (ImGui::GradientButton("ColorBand", &m_biomeColorBand))
               UpdateHeightmapAndTextures();

            ImGui::Checkbox("Use MoistureMap", &m_useMoistureMap);
            if (m_useMoistureMap)
            {
                if (ImGui::GradientButton("Moisture Colors", &m_moistureBiomeColorBand))
                    UpdateHeightmapAndTextures();
            }

            ImGui::EndChild();
            ImGui::BeginChild("Settings buttons", ImVec2(settingsPandelWidth, 93), true);

            //ImGui::Checkbox("Draw Chunks", &m_drawChunkLines);
            //ImGui::Checkbox("Draw Triangles", &m_drawTriangles);

            if (ImGui::Button("Set default params", ImVec2(-1, 0)))
            {
                SetDefaultHeightmapValues();
                UpdateHeightmapAndTextures();
            }

            if (ImGui::Button("Random seed", ImVec2(-1, 0)))
            {
                m_heightmapDesc.device1.noise.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
                m_heightmapDesc.device2.noise.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
                UpdateHeightmapAndTextures();
            }

            if (ImGui::Button("Generate terrain mesh", ImVec2(-1, 0)))
                GenerateTerrainMesh();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Generated on the selected entity objects mesh. Old data is cleared");

            ImGui::EndChild();

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
                const ImVec2 drawPosStart = ImGui::GetCursorScreenPos();
                
                ImVec2 size = ImGui::CalcItemSize(ImVec2(-1, -2), 300, 300);
                size.x = math::Min(size.x, size.y);
                size.y = math::Min(size.x, size.y);
                ImGui::Image((ImTextureID)tex, size);

                //if (m_drawTriangles)
                //    DrawMeshTriangles(drawPosStart);
                if (m_drawChunkLines)
                    DrawChunkLines(drawPosStart);
            }

            ImGui::SameLine(-1, 20);
            gui::ComboBox("Display", m_selectedMapType, g_mapCombo);

            ImGui::EndTable();
        }
    }

    const graphics::Texture* TerrainGenerator::GetTerrainMapTexture(Map type) const
    {
        switch (m_selectedMapType)
        {
        case Map::InputA: return &m_heightmapInputATex;
        case Map::InputB: return &m_heightmapInputBTex;
        case Map::Combined: return &m_heightmapTex;
        default: assert(0);
        }

        return nullptr;
    }

    void TerrainGenerator::SetDefaultHeightmapValues()
    {
        m_heightmapDesc = HeightmapDesc();

        m_heightmapDesc.device1.noise = NoiseDesc();
        m_heightmapDesc.device1.noise.noiseType = NoiseType::Perlin;
        m_heightmapDesc.device1.noise.frequency = 1.389f;
        m_heightmapDesc.device1.noise.octaves = 4;
        m_heightmapDesc.device1.strataOp = HeightmapStrataOp::Smooth;
        m_heightmapDesc.device1.strata = 5.0f;

        m_heightmapDesc.device2.noise.noiseType = NoiseType::Cellular;
        m_heightmapDesc.device2.noise.frequency = 1.859f;
        m_heightmapDesc.device2.noise.octaves = 4;
        m_heightmapDesc.device2.noise.cellularReturnType = CellularReturnType::Distance;
        m_heightmapDesc.device2.strataOp = HeightmapStrataOp::Smooth;
        m_heightmapDesc.device2.strata = 12.0f;

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

    void TerrainGenerator::UpdateHeightmap()
    {
        CYB_TIMED_FUNCTION();
        CreateHeightmap(m_heightmapDesc, m_heightmap);
    }

    static void CreateHeightmapImageTexture(const HeightmapImage& image, graphics::Texture& texture)
    {
        graphics::TextureDesc desc;
        desc.format = graphics::Format::R32_Float;
        desc.components.r = graphics::TextureComponentSwizzle::R;
        desc.components.g = graphics::TextureComponentSwizzle::R;
        desc.components.b = graphics::TextureComponentSwizzle::R;
        desc.width = image.width;
        desc.height = image.height;
        desc.bindFlags = graphics::BindFlags::ShaderResourceBit;

        graphics::SubresourceData subresourceData;
        subresourceData.mem = image.data.data();
        subresourceData.rowPitch = desc.width * graphics::GetFormatStride(graphics::Format::R32_Float);
        
        renderer::GetDevice()->CreateTexture(&desc, &subresourceData, &texture);
    }

    void TerrainGenerator::UpdateTextures()
    {
        CreateHeightmapImageTexture(m_heightmap.inputA, m_heightmapInputATex);
        CreateHeightmapImageTexture(m_heightmap.inputB, m_heightmapInputBTex);
        CreateHeightmapImageTexture(m_heightmap.image, m_heightmapTex);
    }

    void TerrainGenerator::UpdateHeightmapAndTextures()
    {
        UpdateHeightmap();
        UpdateTextures();
    }

    // ========================= TEST =========================
    void ColorizeMountains(scene::MeshComponent* mesh)
    {
        for (size_t i = 0; i < mesh->indices.size(); i += 3)
        {
            uint32_t i0 = mesh->indices[i + 0];
            uint32_t i1 = mesh->indices[i + 1];
            uint32_t i2 = mesh->indices[i + 2];

            const XMFLOAT3& p0 = mesh->vertex_positions[i0];
            const XMFLOAT3& p1 = mesh->vertex_positions[i1];
            const XMFLOAT3& p2 = mesh->vertex_positions[i2];

            XMVECTOR U = XMLoadFloat3(&p2) - XMLoadFloat3(&p0);
            XMVECTOR V = XMLoadFloat3(&p1) - XMLoadFloat3(&p0);

            XMVECTOR N = XMVector3Cross(U, V);
            N = XMVector3Normalize(N);

            XMVECTOR UP = XMVectorSet(0, 1, 0, 0);
            float dp = XMVectorGetX(XMVector3Dot(UP, N));
            if (dp < 0.55f)
            {
                mesh->vertex_colors[i0] = math::StoreColor_RGBA(XMFLOAT4(0.6, 0.6, 0.6, 1));
                mesh->vertex_colors[i1] = math::StoreColor_RGBA(XMFLOAT4(0.6, 0.6, 0.6, 1));
                mesh->vertex_colors[i2] = math::StoreColor_RGBA(XMFLOAT4(0.6, 0.6, 0.6, 1));
            }
        }
    }

    void TerrainGenerator::GenerateTerrainMesh()
    {
        //CYB_TIMED_FUNCTION();

        // Triangulate the heightmap:
        HeightmapTriangulator triangulator(&m_heightmap);
        triangulator.Run(m_meshDesc.maxError, m_meshDesc.maxVertices, m_meshDesc.maxTriangles);
        m_mesh.points = triangulator.GetPoints();
        m_mesh.triangles = triangulator.GetTriangles();
        CYB_TRACE("Updated terrain mesh (numVertices={0}, numTriangles={1})", m_mesh.points.size(), m_mesh.triangles.size());

        // Create scene entities:
        scene::Scene& scene = scene::GetScene();
        ecs::Entity objectID = scene.CreateObject("Terrain01");
        scene::ObjectComponent* object = scene.objects.GetComponent(objectID);
        object->meshID = scene.CreateMesh("Terrain_Mesh");
        scene::MeshComponent* mesh = scene.meshes.GetComponent(object->meshID);

        // Load vertices:
        const XMFLOAT3 positionToCenter = XMFLOAT3(-(m_meshDesc.size * 0.5f), 0.0f, -(m_meshDesc.size * 0.5f));
        mesh->vertex_positions.reserve(m_mesh.points.size());
        mesh->vertex_colors.reserve(m_mesh.points.size());
        for (const auto& p : m_mesh.points)
        {
            mesh->vertex_positions.push_back(XMFLOAT3(
                positionToCenter.x + p.x * m_meshDesc.size,
                positionToCenter.y + math::Lerp(m_meshDesc.minAltitude, m_meshDesc.maxAltitude, p.y),
                positionToCenter.z + p.z * m_meshDesc.size));
            mesh->vertex_colors.push_back(m_biomeColorBand.GetColorAt(p.y));
        }
        
        // Load indices:
        mesh->indices.reserve(m_mesh.triangles.size() * 3);
        for (const auto& tri : m_mesh.triangles)
        {
            mesh->indices.push_back(tri.x);
            mesh->indices.push_back(tri.z);
            mesh->indices.push_back(tri.y);
        }

        ColorizeMountains(mesh);

        // Setup mesh subset and create render data:
        scene::MeshComponent::MeshSubset subset;
        subset.indexOffset = 0;
        subset.indexCount = (uint32_t)mesh->indices.size();
        subset.materialID = scene.CreateMaterial("Terrain_Material");
        mesh->subsets.push_back(subset);

        scene::MaterialComponent* material = scene.materials.GetComponent(subset.materialID);
        material->roughness = 0.82;
        material->metalness = 0.24;

        mesh->CreateRenderData();
    }
}