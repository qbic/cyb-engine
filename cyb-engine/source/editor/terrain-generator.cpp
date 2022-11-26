#include <algorithm>
#include "core/noise.h"
#include "core/profiler.h"
#include "editor/editor.h"
#include "editor/imgui-widgets.h"
#include "editor/terrain-generator.h"

#define CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE() if (ImGui::IsItemDeactivatedAfterEdit()) { UpdateBitmapsAndTextures(); }

namespace cyb::editor
{
    static XMFLOAT4 Biome(float e, float m)
    {
        const XMFLOAT4 COLOR_SNOW = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        const XMFLOAT4 COLOR_MOUNTAIN = XMFLOAT4(0.53f, 0.345f, 0.0f, 1.0f);
        const XMFLOAT4 COLOR_DARK_MOUNTAIN = XMFLOAT4(0.404f, 0.263f, 0.0f, 1.0f);
        const XMFLOAT4 COLOR_GRASS = XMFLOAT4(0.0f, 0.68f, 0.018f, 1.0f);
        const XMFLOAT4 COLOR_DARK_GRASS = XMFLOAT4(0.154f, 0.432f, 0.031f, 1.0f);
        const XMFLOAT4 COLOR_SAND = XMFLOAT4(0.96f, 0.63f, 0.51f, 1.0f);
        const XMFLOAT4 COLOR_WATER = XMFLOAT4(0.0f, 0.235f, 1.0f, 1.0f);

        if (e < 0.06) return COLOR_WATER;
        if (e < 0.05)
        {
            if (m > 0.85) return COLOR_GRASS;
            return COLOR_SAND;
        }

        if (e > 0.84) return COLOR_SNOW;

        if (e > 0.35)
        {
            if (m > 0.55) return COLOR_DARK_MOUNTAIN;
            return COLOR_MOUNTAIN;
        }

        if (e > 0.32)
        {
            if (m > 0.9) return COLOR_DARK_GRASS;
            if (m > 0.7) return COLOR_GRASS;
            if (m > 0.6) return COLOR_DARK_MOUNTAIN;
            return COLOR_MOUNTAIN;
        }

        if (m > 0.5f) return COLOR_DARK_GRASS;
        return COLOR_GRASS;
    }

    void CreateTerrainBitmap(jobsystem::Context& ctx, const TerrainBitmapDesc& desc, TerrainBitmap& bitmap)
    {
        bitmap.desc = desc;
        bitmap.image.clear();
        bitmap.image.resize(desc.width * desc.height);
        bitmap.maxN = std::numeric_limits<float>::min();
        bitmap.minN = std::numeric_limits<float>::max();

        jobsystem::Dispatch(ctx, desc.height, 128, [&](jobsystem::JobArgs args)
            {
                NoiseGenerator noise(desc.seed);
                noise.SetFrequency(desc.frequency);
                noise.SetInterp(desc.interp);
                noise.SetFractalOctaves(desc.octaves);

                const uint32_t y = args.jobIndex;
                for (uint32_t x = 0; x < desc.width; ++x)
                {
                    const float xs = (float)x / (float)desc.width;
                    const float ys = (float)y / (float)desc.height;
                    const size_t offset = (size_t)y * desc.width + x;

                    float value = noise.GetNoise(xs, ys);

                    // Apply strtata:
                    switch (desc.strataFunc)
                    {
                    case TerrainStrata::SharpSub:
                    {
                        const float steps = -math::Abs(std::sin(value * desc.strata * math::M_PI) * (0.1f / desc.strata * math::M_PI));
                        value = (value * 0.5f + steps * 0.5f);
                    } break;
                    case TerrainStrata::SharpAdd:
                    {
                        const float steps = math::Abs(std::sin(value * desc.strata * math::M_PI) * (0.1f / desc.strata * math::M_PI));
                        value = (value * 0.5f + steps * 0.5f);

                    } break;
                    case TerrainStrata::Quantize:
                    {
                        const float strata = desc.strata * 2.0f;
                        value = int(value * strata) * 1.0f / strata;
                    } break;
                    case TerrainStrata::Smooth:
                    {
                        const float strata = desc.strata * 2.0f;
                        const float steps = std::sin(value * strata * math::M_PI) * (0.1f / strata * math::M_PI);
                        value = value * 0.5f + steps * 0.5f;
                    } break;
                    case TerrainStrata::None:
                    default:
                        break;
                    }

                    // Get min/max noise values for height normalization stage:
                    bitmap.maxN = math::Max(bitmap.maxN.load(), value);
                    bitmap.minN = math::Min(bitmap.minN.load(), value);
                    bitmap.image[offset] = value;
                }
            });
    }

    void NormalizeTerrainBitmapValues(jobsystem::Context& ctx, TerrainBitmap& bitmap)
    {
        // Normalize bitmap values to [0..1] range
        if (bitmap.desc.normalize)
        {
            jobsystem::Dispatch(ctx, bitmap.desc.height, 128, [&](jobsystem::JobArgs args)
                {
                    const float scale = 1.0f / (bitmap.maxN - bitmap.minN);
                    const uint32_t y = args.jobIndex;
                    for (uint32_t x = 0; x < bitmap.desc.width; ++x)
                    {
                        const size_t offset = (size_t)y * bitmap.desc.width + x;
                        const float value = bitmap.image[offset];
                        bitmap.image[offset] = math::Saturate((value - bitmap.minN) * scale);
                    }
                });
        }
    }

    void CreateTerrainColormap(jobsystem::Context& ctx, const TerrainBitmap& height, const TerrainBitmap& moisture, std::vector<uint32_t>& color)
    {
        assert(height.desc.width == moisture.desc.width);
        assert(height.desc.height == moisture.desc.height);
        assert(height.image.size() == height.desc.width * height.desc.height);
        assert(moisture.image.size() == moisture.desc.width * moisture.desc.height);

        color.clear();
        color.resize(height.image.size());

        jobsystem::Dispatch(ctx, height.desc.height, 128, [&](jobsystem::JobArgs args)
            {
                uint32_t y = args.jobIndex;
                for (uint32_t x = 0; x < height.desc.width; ++x)
                {
                    const size_t offset = (size_t)y * height.desc.height + x;
                    const float h = height.image[offset];
                    const float m = moisture.image[offset];
                    color[offset] = math::StoreColor_RGBA(Biome(h, m));
                }
            });
    }

    void CreateTerrainColormap2(jobsystem::Context& ctx, const TerrainBitmap& height, const ImGradient& colorBand, std::vector<uint32_t>& color)
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

    static ecs::Entity terrain_object_id = ecs::InvalidEntity;
    static ecs::Entity terrain_material_id = ecs::InvalidEntity;
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

    static const std::unordered_map<NoiseInterpolation, std::string> g_interpCombo = {
        { NoiseInterpolation::Linear,               "Linear"   },
        { NoiseInterpolation::Hermite,              "Hermite"  },
        { NoiseInterpolation::Quintic,              "Quintic"  }
    };

    static const std::unordered_map<TerrainStrata, std::string> g_strataFuncCombo = {
        { TerrainStrata::None,                      "None"     },
        { TerrainStrata::SharpSub,                  "SharpSub" },
        { TerrainStrata::SharpAdd,                  "SharpAdd" },
        { TerrainStrata::Quantize,                  "Quantize" },
        { TerrainStrata::Smooth,                    "Smooth"   }
    };

    TerrainGenerator::TerrainGenerator()
    {
        m_biomeColorBand = {
            { ImColor(0,   20,  122), 0.000f },
            { ImColor(78,  62,   27), 0.100f },
            { ImColor(173, 137,  59), 0.142f },
            { ImColor( 16, 109,  27), 0.185f },
            { ImColor( 29, 191,  38), 0.312f },
            { ImColor( 16, 109,  27), 0.559f },
            { ImColor( 94,  94,  94), 0.607f },
            { ImColor( 75,  75,  75), 0.798f },
            { ImColor(255, 255, 255), 0.921f }
        };
    }

    void TerrainGenerator::DrawGui(ecs::Entity selectedEntity)
    {
        const char* tableID = "TerrainGeneratorOptionssTable";
        const float optionsColumnWidth = 280.0f;

        if (!m_initialized)
        {
            UpdateBitmapsAndTextures();
            m_initialized = true;
        }

        ImGui::BeginTable("TerrainGenerator", 2, 0);
        ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, optionsColumnWidth);
        ImGui::TableNextColumn();

        const int tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable(tableID, 2, tableFlags))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
            CYB_GUI_COMPONENT(ImGui::DragFloat, "Map Size", &m_meshDesc.size, 1.0f, 1.0f, 10000.0f, "%.2fm");
            CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
            CYB_GUI_COMPONENT(ImGui::DragFloat, "Min Altitude", &m_meshDesc.minAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
            CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
            CYB_GUI_COMPONENT(ImGui::DragFloat, "Max Altitude", &m_meshDesc.maxAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
            CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
            CYB_GUI_COMPONENT(ImGui::DragInt, "Resolution", (int*)&m_meshDesc.mapResolution, 1.0f, 1, 2048), "%dpx";
            CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
            CYB_GUI_COMPONENT(ImGui::SliderInt, "NumChunks", (int*)&m_meshDesc.numChunks, 1, 32, "%d^2");
            CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
            ImGui::EndTable();
        }
        ImGui::Separator();

        auto editTerrainBitmap = [&](const char* label_, TerrainBitmapDesc& bitmapDesc)
        {
            ImGui::TextUnformatted(label_);
            if (ImGui::BeginTable(tableID, 2, tableFlags))
            {
                if (CYB_GUI_COMPONENT(gui::ComboBox, "Interpolation", bitmapDesc.interp, g_interpCombo))
                {
                    UpdateBitmapsAndTextures();
                }
                CYB_GUI_COMPONENT(ImGui::DragInt, "Seed", (int*)&bitmapDesc.seed, 1.0f, 0, std::numeric_limits<int>::max());
                CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
                CYB_GUI_COMPONENT(ImGui::SliderFloat, "Frequency", &bitmapDesc.frequency, 0.0f, 10.0f);
                CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
                CYB_GUI_COMPONENT(ImGui::SliderInt, "FBM Octaves", (int*)&bitmapDesc.octaves, 1, 8);
                CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();

                if (CYB_GUI_COMPONENT(gui::ComboBox, "Strata", bitmapDesc.strataFunc, g_strataFuncCombo))
                {
                    UpdateBitmapsAndTextures();
                }
                if (bitmapDesc.strataFunc != TerrainStrata::None)
                {
                    CYB_GUI_COMPONENT(ImGui::SliderFloat, "Amount", &bitmapDesc.strata, 1.0f, 15.0f);
                    CYB_REFRESH_TEXTURES_ON_ITEM_CHANGE();
                }

                ImGui::EndTable();
            }
            ImGui::Separator();
        };

        editTerrainBitmap("HeightMap Description", m_heightmapDesc);
        editTerrainBitmap("MoistureMap Description", m_moisturemapDesc);
        ImGui::Spacing();

        if (ImGui::BeginTable(tableID, 2, tableFlags))
        {
            if (CYB_GUI_COMPONENT(ImGui::GradientButton, "ColorBand", &m_biomeColorBand))
            {
                UpdateColormapAndTextures();
            }

            CYB_GUI_COMPONENT(ImGui::Checkbox, "Use MoistureMap", &m_useMoistureMap);
            if (m_useMoistureMap)
            {
                if (CYB_GUI_COMPONENT(ImGui::GradientButton, "Moisture Colors", &m_moistureBiomeColorBand))
                {
                    UpdateColormapAndTextures();
                }
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::BeginTable(tableID, 2, tableFlags))
        {
            CYB_GUI_COMPONENT(gui::ComboBox, "Map Display", m_selectedMapType, g_mapCombo);
            CYB_GUI_COMPONENT(ImGui::Checkbox, "Draw Chunks", &m_drawChunkLines);
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Set default params", ImVec2(-1, 0)))
        {
            m_meshDesc = TerrainMeshDesc();
            UpdateBitmapsAndTextures();
        }

        if (ImGui::Button("Random seed", ImVec2(-1, 0)))
        {
            m_heightmapDesc.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
            m_moisturemapDesc.seed = random::GenerateInteger(0, std::numeric_limits<int>::max());
            UpdateBitmapsAndTextures();
        }

#if 0
        if (ImGui::Button("Generate terrain texture", ImVec2(-1, 0)) || !m_heightmapTex.IsValid())
        {
            UpdateBitmaps();
            UpdateBitmapTextures();
    }
#endif
        if (ImGui::Button("Generate terrain mesh", ImVec2(-1, 0)) || !m_heightmapTex.IsValid())
        {
            scene::Scene& scene = scene::GetScene();
            //GenerateTerrain(desc, &scene);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Generated on the selected entity objects mesh. Old data is cleared");
        }

        //
        // Display the generated image
        //
        ImGui::TableNextColumn();
        const graphics::Texture* tex = GetTerrainMapTexture(m_selectedMapType);

        if (tex->IsValid())
        {
            ImVec2 size = ImGui::CalcItemSize(ImVec2(-1, -2), 300, 300);
            size.x = math::Min(size.x, size.y);
            size.y = math::Min(size.x, size.y);

            ImVec2 chunkLinesPos = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)tex, size);

            if (m_drawChunkLines)
            {
                DrawChunkLines(chunkLinesPos);
            }
        }

        ImGui::EndTable();
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

    void TerrainGenerator::UpdateBitmaps()
    {
        CYB_TIMED_FUNCTION();

        m_heightmapDesc.width = m_heightmapDesc.height = m_meshDesc.mapResolution;
        m_moisturemapDesc.width = m_moisturemapDesc.height = m_meshDesc.mapResolution;

        jobsystem::Context ctx;
        CreateTerrainBitmap(ctx, m_heightmapDesc, m_heightmap);
        CreateTerrainBitmap(ctx, m_moisturemapDesc, m_moisturemap);
        jobsystem::Wait(ctx);
        NormalizeTerrainBitmapValues(ctx, m_heightmap);
        NormalizeTerrainBitmapValues(ctx, m_moisturemap);
        jobsystem::Wait(ctx);
        UpdateColormap(ctx);
        jobsystem::Wait(ctx);
    }

    void TerrainGenerator::UpdateColormap(jobsystem::Context& ctx)
    {
        //CreateTerrainColormap(ctx, m_heightmap, m_moisturemap, m_colormap);
        CreateTerrainColormap2(ctx, m_heightmap, m_biomeColorBand, m_colormap);
    }

    void TerrainGenerator::UpdateColormapAndTextures()
    {
        jobsystem::Context ctx;
        UpdateColormap(ctx);
        jobsystem::Wait(ctx);
        UpdateBitmapTextures();
    }

    void TerrainGenerator::UpdateBitmapTextures()
    {
        //CYB_TIMED_FUNCTION();

        graphics::TextureDesc texDesc;
        graphics::SubresourceData subresourceData;

        texDesc.format = graphics::Format::R32_Float;
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
        subresourceData.mem = m_colormap.data();
        subresourceData.rowPitch = texDesc.width * graphics::GetFormatStride(graphics::Format::R8G8B8A8_Unorm);
        renderer::GetDevice()->CreateTexture(&texDesc, &subresourceData, &m_colormapTex);
    }

    void TerrainGenerator::UpdateBitmapsAndTextures()
    {
        UpdateBitmaps();
        UpdateBitmapTextures();
    }
}