#include <algorithm>
#include <fstream>
#include "core/noise.h"
#include "core/random.h"
#include "core/profiler.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "systems/event-system.h"
#include "editor/editor.h"
#include "editor/terrain-generator.h"
#include "editor/undo-manager.h"
#include "editor/icons_font_awesome6.h"
#include "json.hpp"

namespace cyb::editor
{
    static const std::unordered_map<StrataOp, std::string> g_strataFuncCombo2 = {
        { StrataOp::None,                       "None"     },
        { StrataOp::SharpSub,                   "SharpSub" },
        { StrataOp::SharpAdd,                   "SharpAdd" },
        { StrataOp::Quantize,                   "Quantize" },
        { StrataOp::Smooth,                     "Smooth"   }
    };

    static const std::unordered_map<noise::Type, std::string> g_noiseTypeCombo = {
        { noise::Type::Perlin,                  "Perlin"    },
        { noise::Type::Cellular,                "Cellular"  }
    };

    static const std::unordered_map<MixOp, std::string> g_mixOpCombo = {
        { MixOp::Add,                           "Add"       },
        { MixOp::Sub,                           "Sub"       },
        { MixOp::Mul,                           "Mul"       },
        { MixOp::Lerp,                          "Lerp"      }
    };

    static const std::unordered_map<noise::CellularReturn, std::string> g_cellularReturnTypeCombo = {
        { noise::CellularReturn::CellValue,     "CellValue"     },
        { noise::CellularReturn::Distance,      "Distance"      },
        { noise::CellularReturn::Distance2,     "Distance2"     },
        { noise::CellularReturn::Distance2Add,  "Distance2Add"  },
        { noise::CellularReturn::Distance2Sub,  "Distance2Sub"  },
        { noise::CellularReturn::Distance2Mul,  "Distance2Mul"  },
        { noise::CellularReturn::Distance2Div,  "Distance2Div"  }
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
        CreatePreviewImage();
    }

    void TerrainGenerator::SetDefaultHeightmapValues()
    {
        heightmap.inputList = GetDefaultInputs();
        heightmap.UnlockMinMax();
        heightmap.LockMinMax();
    }

    void TerrainGenerator::DrawGui(ecs::Entity selectedEntity)
    {
        static const uint32_t settingsPandelWidth = 340;

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Settings", ""))
                {
                    filesystem::SaveDialog("JSON (*.json)\0*.json\0", [&](std::string filename) {
                        nlohmann::ordered_json json;
                        auto& terrain = json["terrain_settings"];
#if 0
                        terrain["mesh"]["size"] = m_meshDesc.size;
                        terrain["mesh"]["maxAltitude"] = m_meshDesc.maxAltitude;
                        terrain["mesh"]["minAltitude"] = m_meshDesc.minAltitude;
                        terrain["mesh"]["numChunks"] = m_meshDesc.numChunks;
                        terrain["mesh"]["maxError"] = m_meshDesc.maxError;

                        terrain["heightmap"]["width"] = m_heightmapDesc.width;
                        terrain["heightmap"]["height"] = m_heightmapDesc.height;
                        terrain["heightmap"]["combineType"] = m_heightmapDesc.combineType;
                        terrain["heightmap"]["combineStrength"] = m_heightmapDesc.combineStrength;
                        terrain["heightmap"]["exponent"] = m_heightmapDesc.exponent;

                        auto& input1 = terrain["heightmap"]["input1"];
                        input1["noise"]["type"] = m_heightmapDesc.device1.noise.type;
                        input1["noise"]["seed"] = m_heightmapDesc.device1.noise.seed;
                        input1["noise"]["frequency"] = m_heightmapDesc.device1.noise.frequency;
                        input1["noise"]["octaves"] = m_heightmapDesc.device1.noise.octaves;
                        input1["noise"]["lacunarity"] = m_heightmapDesc.device1.noise.lacunarity;
                        input1["noise"]["gain"] = m_heightmapDesc.device1.noise.gain;
                        input1["noise"]["cellularReturnType"] = m_heightmapDesc.device1.noise.cellularReturnType;
                        input1["noise"]["cellularJitterModifier"] = m_heightmapDesc.device1.noise.cellularJitterModifier;
                        input1["strataOp"] = m_heightmapDesc.device1.strataOp;
                        input1["strata"] = m_heightmapDesc.device1.strata;
                        input1["blur"] = m_heightmapDesc.device1.blur;

                        auto& input2 = terrain["heightmap"]["input2"];
                        input2["noise"]["type"] = m_heightmapDesc.device2.noise.type;
                        input2["noise"]["seed"] = m_heightmapDesc.device2.noise.seed;
                        input2["noise"]["frequency"] = m_heightmapDesc.device2.noise.frequency;
                        input2["noise"]["octaves"] = m_heightmapDesc.device2.noise.octaves;
                        input2["noise"]["lacunarity"] = m_heightmapDesc.device2.noise.lacunarity;
                        input2["noise"]["gain"] = m_heightmapDesc.device2.noise.gain;
                        input2["noise"]["cellularReturnType"] = m_heightmapDesc.device2.noise.cellularReturnType;
                        input2["noise"]["cellularJitterModifier"] = m_heightmapDesc.device2.noise.cellularJitterModifier;
                        input2["strataOp"] = m_heightmapDesc.device2.strataOp;
                        input2["strata"] = m_heightmapDesc.device2.strata;
                        input2["blur"] = m_heightmapDesc.device2.blur;
#endif
                        if (!filesystem::HasExtension(filename, "json"))
                            filename += ".json";

                        std::ofstream o(filename);
                        o << std::setw(4) << json << std::endl;
                        });
                }
                if (ImGui::MenuItem("Load Settings", ""))
                {
                    filesystem::OpenDialog("JSON (*.json)\0*.json\0", [&](std::string filename) {
                        std::ifstream f(filename);

                        nlohmann::json json = nlohmann::json::parse(f);
#if 0
                        auto& terrain = json["terrain_settings"];
                        m_meshDesc.size = terrain["mesh"]["size"];
                        m_meshDesc.maxAltitude = terrain["mesh"]["maxAltitude"];
                        m_meshDesc.minAltitude = terrain["mesh"]["minAltitude"];
                        m_meshDesc.numChunks = terrain["mesh"]["numChunks"];
                        m_meshDesc.maxError = terrain["mesh"]["maxError"];
                        m_heightmapDesc.width = terrain["heightmap"]["width"];
                        m_heightmapDesc.height = terrain["heightmap"]["height"];
                        m_heightmapDesc.combineType = terrain["heightmap"]["combineType"];
                        m_heightmapDesc.combineStrength = terrain["heightmap"]["combineStrength"];
                        m_heightmapDesc.exponent = terrain["heightmap"]["exponent"];

                        auto& input1 = terrain["heightmap"]["input1"];
                        m_heightmapDesc.device1.noise.type = input1["noise"]["type"];
                        m_heightmapDesc.device1.noise.seed = input1["noise"]["seed"];
                        m_heightmapDesc.device1.noise.frequency = input1["noise"]["frequency"];
                        m_heightmapDesc.device1.noise.octaves = input1["noise"]["octaves"];
                        m_heightmapDesc.device1.noise.lacunarity = input1["noise"]["lacunarity"];
                        m_heightmapDesc.device1.noise.gain = input1["noise"]["gain"];
                        m_heightmapDesc.device1.noise.cellularReturnType = input1["noise"]["cellularReturnType"];
                        m_heightmapDesc.device1.noise.cellularJitterModifier = input1["noise"]["cellularJitterModifier"];
                        m_heightmapDesc.device1.strataOp = input1["strataOp"];
                        m_heightmapDesc.device1.strata = input1["strata"];
                        m_heightmapDesc.device1.blur = input1["blur"];

                        auto& input2 = terrain["heightmap"]["input2"];
                        m_heightmapDesc.device2.noise.type = input2["noise"]["type"];
                        m_heightmapDesc.device2.noise.seed = input2["noise"]["seed"];
                        m_heightmapDesc.device2.noise.frequency = input2["noise"]["frequency"];
                        m_heightmapDesc.device2.noise.octaves = input2["noise"]["octaves"];
                        m_heightmapDesc.device2.noise.lacunarity = input2["noise"]["lacunarity"];
                        m_heightmapDesc.device2.noise.gain = input2["noise"]["gain"];
                        m_heightmapDesc.device2.noise.cellularReturnType = input2["noise"]["cellularReturnType"];
                        m_heightmapDesc.device2.noise.cellularJitterModifier = input2["noise"]["cellularJitterModifier"];
                        m_heightmapDesc.device2.strataOp = input2["strataOp"];
                        m_heightmapDesc.device2.strata = input2["strata"];
                        m_heightmapDesc.device2.blur = input2["blur"];
#endif
                        CreatePreviewImage();
                        ui::GetUndoManager().ClearActionHistory();
                        });
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "CTRL+Z"))
                    ui::GetUndoManager().Undo();
                if (ImGui::MenuItem("Redo", "CTRL+Y"))
                    ui::GetUndoManager().Redo();
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        const auto updatePreview = [this]() { CreatePreviewImage(); };

        if (ImGui::BeginTable("TerrainGenerator", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoClip);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);

            // begin drawing of the settings panel
            ImGui::TableNextColumn();
            ImGui::BeginChild("Settings panel", ImVec2(settingsPandelWidth, 680), ImGuiChildFlags_Border);

            if (ImGui::CollapsingHeader("Mesh Settings"))
            {
                ui::DragInt("ChunkSize", &m_meshDesc.size, 1, 1, 2000, "%dm");
                ui::SliderInt("Expand", (int*)&m_meshDesc.chunkExpand, nullptr, 0, 24);
                ui::DragFloat("Min Altitude", &m_meshDesc.minAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
                ui::DragFloat("Max Altitude", &m_meshDesc.maxAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
                ui::SliderFloat("Max Error", &m_meshDesc.maxError, nullptr, 0.0001f, 0.01f, "%.4f");
            }

            if (ImGui::CollapsingHeader("Heightmap Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t i = 0; i < heightmap.inputList.size(); i++)
                {
                    HeightmapGenerator::Input& input = heightmap.inputList[i];

                    const ui::IdScopeGuard idGuard(&input);
                    ui::ComboBox("NoiseType", input.noise.type, g_noiseTypeCombo, updatePreview);
                    if (input.noise.type == noise::Type::Cellular)
                    {
                        ui::ComboBox("CelReturn", input.noise.cellularReturnType, g_cellularReturnTypeCombo, updatePreview);
                    }

                    ui::SliderInt("Seed", (int*)&input.noise.seed, updatePreview, 0, std::numeric_limits<int>::max() / 2);
                    ui::SliderFloat("Frequency", &input.noise.frequency, updatePreview, 0.0f, 10.0f);
                    ui::ComboBox("StrataOp", input.strataOp, g_strataFuncCombo2, updatePreview);
                    if (input.strataOp != StrataOp::None)
                    {
                        ui::SliderFloat("Strata", &input.strata, updatePreview, 1.0f, 15.0f);
                    }
                    ui::SliderFloat("Exponent", &input.exponent, updatePreview, 0.0f, 4.0f);

                    if (ImGui::TreeNode("FBM"))
                    {
                        ui::SliderInt("Octaves", (int*)&input.noise.octaves, updatePreview, 1, 8);
                        ui::SliderFloat("Lacunarity", &input.noise.lacunarity, updatePreview, 0.0f, 4.0f);
                        ui::SliderFloat("Gain", &input.noise.gain, updatePreview, 0.0f, 4.0f);
                        ImGui::TreePop();
                    }

                    if (i + 1 < heightmap.inputList.size())
                    {
                        ui::ComboBox("MixOp", input.mixOp, g_mixOpCombo, updatePreview);
                        if (input.mixOp == MixOp::Lerp)
                        {
                            ui::SliderFloat("Mix", &input.mixing, updatePreview, 0.0f, 1.0f);
                        }
                    }

                    ImGui::Separator();
                }
            }

            ImGui::Spacing();
            if (ui::GradientButton("ColorBand", &m_biomeColorBand))
            {
                CreatePreviewImage();
            }

            ImGui::EndChild();
            ImGui::BeginChild("Settings buttons", ImVec2(settingsPandelWidth, 130), ImGuiChildFlags_Border);

            if (ImGui::Button("Set Default Params", ImVec2(-1, 0)))
            {
                SetDefaultHeightmapValues();
                CreatePreviewImage();
            }

            if (ImGui::Button("Random seed", ImVec2(-1, 0)))
            {
                for (auto& input : heightmap.inputList)
                {
                    input.noise.seed = random::GetNumber<uint32_t>(0, std::numeric_limits<int>::max());
                }

                CreatePreviewImage();
            }

            if (!jobsystem::IsBusy(jobContext))
            {
                if (ImGui::Button("Generate terrain mesh", ImVec2(-1, 0)))
                    GenerateTerrainMesh();
            }
            else 
            {
                if (ImGui::Button("[Cancel generation]", ImVec2(-1, 0)))
                    cancelTerrainGen.store(true);
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Generated on the selected entity objects mesh. Old data is cleared");

            ImGui::EndChild();

            // in the second column, display a preview of the terrain as an image
            ImGui::TableNextColumn();
            if (previewTex.IsValid())
            {
                ImGui::PushItemWidth(300);
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const float size = math::Min(available.x - 4, available.y - 4);
                const ImVec2 p0 = ImGui::GetCursorScreenPos();
                const ImVec2 p1 = ImVec2(p0.x + size, p0.y + size);

                ImGui::InvisibleButton("##HeightPreviewImage", ImVec2(size, size));
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddImage((ImTextureID)&previewTex, p0, p1);
                const std::string info = fmt::format("offset ({}, {}), use left mouse button to drag", previewOffset.x, previewOffset.y);
                drawList->AddText(ImVec2(p0.x + 8, p0.y + 8), 0xFFFFFFFF, info.c_str());

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ImGuiIO& io = ImGui::GetIO();
                    previewOffset.x -= static_cast<int>(io.MouseDelta.x);
                    previewOffset.y -= static_cast<int>(io.MouseDelta.y);
                    updatePreview();
                }
                ImGui::PopItemWidth();
            }

            ImGui::EndTable();
        }
    }

    void TerrainGenerator::CreatePreviewImage()
    {
        // generate preview texture aswell as generating some min
        // max values in heightmap for psuedo normalization
        jobsystem::Wait(previewGenContext);
        jobsystem::Execute(previewGenContext, [&](jobsystem::JobArgs)
        {
            heightmap.UnlockMinMax();
            std::vector<float> image;
            image.resize(previewResolution * previewResolution);
            for (int32_t i = 0; i < previewResolution; i++)
            {
                for (int32_t j = 0; j < previewResolution; j++)
                {
                    const float scale = (1.0f / (float)previewResolution) * 1024.0f;
                    int sampleX = (int)std::round((float)(j + previewOffset.x) * scale);
                    int sampleY = (int)std::round((float)(i + previewOffset.y) * scale);

                    image[i * previewResolution + j] = heightmap.GetHeightAt(XMINT2(sampleX, sampleY));
                }
            }
            heightmap.LockMinMax();
            //CYB_TRACE("MIN={} MAX={}", heightmap.minValue, heightmap.maxValue);

            // normalize preview image values to 0.0f - 1.0f range
            const float scale = 1.0f / (heightmap.maxValue - heightmap.minValue);
            for (uint32_t i = 0; i < (uint32_t)(previewResolution * previewResolution); ++i)
            {
                image[i] = (image[i] - heightmap.minValue) * scale;
            }

            // create a one channel float texture with all components
            // swizzled for easy grayscale viewing
            graphics::TextureDesc desc;
            desc.format = graphics::Format::R32_Float;
            desc.components.r = graphics::TextureComponentSwizzle::R;
            desc.components.g = graphics::TextureComponentSwizzle::R;
            desc.components.b = graphics::TextureComponentSwizzle::R;
            desc.width = previewResolution;
            desc.height = previewResolution;
            desc.bindFlags = graphics::BindFlags::ShaderResourceBit;

            graphics::SubresourceData subresourceData;
            subresourceData.mem = image.data();
            subresourceData.rowPitch = desc.width * graphics::GetFormatStride(graphics::Format::R32_Float);

            graphics::GetDevice()->CreateTexture(&desc, &subresourceData, &previewTex);
        });
    }

    // Colorize vertical faces with rock color.
    // Indices are re-ordered, first are the ground indices, then the rock indices.
    // Returns the number of ground indices (offset for rock indices)
    uint32_t ColorizeMountains(scene::MeshComponent* mesh)
    {
        const uint32_t rockColor = math::StoreColor_RGBA(XMFLOAT4(0.6, 0.6, 0.6, 1));

        for (size_t i = 0; i < mesh->indices.size(); i += 3)
        {
            const uint32_t i0 = mesh->indices[i + 0];
            const uint32_t i1 = mesh->indices[i + 1];
            const uint32_t i2 = mesh->indices[i + 2];

            const XMFLOAT3& p0 = mesh->vertex_positions[i0];
            const XMFLOAT3& p1 = mesh->vertex_positions[i1];
            const XMFLOAT3& p2 = mesh->vertex_positions[i2];

            const XMVECTOR U = XMLoadFloat3(&p2) - XMLoadFloat3(&p0);
            const XMVECTOR V = XMLoadFloat3(&p1) - XMLoadFloat3(&p0);
            const XMVECTOR N = XMVector3Normalize(XMVector3Cross(U, V));
            const XMVECTOR UP = XMVectorSet(0, 1, 0, 0);

            if (XMVectorGetX(XMVector3Dot(UP, N)) < 0.55f)
            {
                mesh->vertex_colors[i0] = rockColor;
                mesh->vertex_colors[i1] = rockColor;
                mesh->vertex_colors[i2] = rockColor;
            }
        }

        // Loop though all indices and seperate ground from rock indices
        std::vector<uint32_t> groundIndices;
        std::vector<uint32_t> rockIndices;
        groundIndices.resize(mesh->indices.size());
        rockIndices.resize(mesh->indices.size());

        size_t groundIndexCount = 0;
        size_t rockIndexCount = 0;

        for (size_t i = 0; i < mesh->indices.size(); i += 3)
        {
            const uint32_t i0 = mesh->indices[i + 0];
            const uint32_t i1 = mesh->indices[i + 1];
            const uint32_t i2 = mesh->indices[i + 2];

            uint32_t& c = mesh->vertex_colors[i0];
            if (mesh->vertex_colors[i1] == mesh->vertex_colors[i2])
                c = mesh->vertex_colors[i1];

            if (c != rockColor)
            {
                groundIndices[groundIndexCount + 0] = i0;
                groundIndices[groundIndexCount + 1] = i1;
                groundIndices[groundIndexCount + 2] = i2;
                groundIndexCount += 3;
            }
            else
            {
                rockIndices[rockIndexCount + 0] = i0;
                rockIndices[rockIndexCount + 1] = i1;
                rockIndices[rockIndexCount + 2] = i2;
                rockIndexCount += 3;
            }
        }

        groundIndices.resize(groundIndexCount);
        rockIndices.resize(rockIndexCount);

        // Merge all indices, first ground, than rock indices
        mesh->indices = groundIndices;
        if (!rockIndices.empty())
            mesh->indices.insert(mesh->indices.end(), rockIndices.begin(), rockIndices.end());
        return (uint32_t)groundIndices.size();
    }

    void TerrainGenerator::RemoveTerrainData()
    {
        scene::Scene& scene = scene::GetScene();
        chunks.clear();
        scene.RemoveEntity(terrainEntity, true);
    }

    void TerrainGenerator::GenerateTerrainMesh()
    {
        RemoveTerrainData();

        auto requestChunk = [&](int32_t xOffset, int32_t zOffset)
        {
            Chunk chunk = { centerChunk.x + xOffset, centerChunk.z + zOffset };
            auto it = chunks.find(chunk);
            if (it != chunks.end() && it->second.entity != ecs::INVALID_ENTITY)
                return;

            // generate a new chunk
            ChunkData& chunkData = chunks[chunk];
            scene::Scene& chunkScene = chunkData.scene;
            chunkData.entity = chunkScene.CreateObject(fmt::format("Chunk_{}_{}", xOffset, zOffset));
            scene::ObjectComponent* object = chunkScene.objects.GetComponent(chunkData.entity);

            scene::TransformComponent* transform = chunkScene.transforms.GetComponent(chunkData.entity);
            transform->Translate(XMFLOAT3((float)(xOffset * m_meshDesc.size), 0, (float)(zOffset * m_meshDesc.size)));
            transform->UpdateTransform();

            scene::MeshComponent* mesh = &chunkScene.meshes.Create(chunkData.entity);
            object->meshID = chunkData.entity;

            // generate triangulated heightmap mesh
            XMINT2 heightmapOffset = XMINT2(xOffset * m_meshDesc.size, zOffset * m_meshDesc.size);
            HeightmapTriangulator triangulator(&heightmap, m_meshDesc.size, m_meshDesc.size, heightmapOffset);
            triangulator.Run(m_meshDesc.maxError, m_meshDesc.maxVertices, m_meshDesc.maxTriangles);

            jobsystem::Context ctx;
            std::vector<XMFLOAT3> points;
            jobsystem::Execute(ctx, [&](jobsystem::JobArgs args)
            {
                points = triangulator.GetPoints();
            });

            std::vector<XMINT3> triangles;
            jobsystem::Execute(ctx, [&](jobsystem::JobArgs args)
            {
                triangles = triangulator.GetTriangles();
            });

            jobsystem::Wait(ctx);

            // load mesh vertices
            const XMFLOAT3 offsetToCenter = XMFLOAT3(-(m_meshDesc.size * 0.5f), 0.0f, -(m_meshDesc.size * 0.5f));
            mesh->vertex_positions.resize(points.size());
            mesh->vertex_colors.resize(points.size());
            jobsystem::Dispatch(ctx, (uint32_t)points.size(), 512, [&](jobsystem::JobArgs args)
            {
                const uint32_t index = args.jobIndex;
                mesh->vertex_positions[index] = XMFLOAT3(
                    offsetToCenter.x + points[index].x * m_meshDesc.size,
                    offsetToCenter.y + math::Lerp(m_meshDesc.minAltitude, m_meshDesc.maxAltitude, points[index].y),
                    offsetToCenter.z + points[index].z * m_meshDesc.size);
                mesh->vertex_colors[index] = m_biomeColorBand.GetColorAt(points[index].y);
            });

            // load mesh indexes
            mesh->indices.resize(triangles.size() * 3);
            jobsystem::Dispatch(ctx, (uint32_t)triangles.size(), 512, [&](jobsystem::JobArgs args)
            {
                const uint32_t index = args.jobIndex;
                mesh->indices[(index * 3) + 0] = triangles[index].x;
                mesh->indices[(index * 3) + 1] = triangles[index].z;
                mesh->indices[(index * 3) + 2] = triangles[index].y;
            });

            // seperate rock surfaces from ground to enable them
            // to have different materials
            jobsystem::Wait(ctx);
            uint32_t groundIndices = ColorizeMountains(mesh);

            // setup a subset using ground material
            scene::MeshComponent::MeshSubset subset;
            subset.indexOffset = 0;
            subset.indexCount = (uint32_t)groundIndices;
            subset.materialID = groundMateral;
            mesh->subsets.push_back(subset);

            // setup subset using rock material
            subset.indexOffset = groundIndices;
            subset.indexCount = (uint32_t)mesh->indices.size() - groundIndices;
            subset.materialID = rockMatereal;
            mesh->subsets.push_back(subset);

            mesh->CreateRenderData();
        };

        scene::Scene& scene = scene::GetScene();

        // create terrain group entity
        terrainEntity = scene.CreateGroup("Terrain");

        // create terrain materials
        groundMateral = scene.CreateMaterial("TerrainGround_Material");
        scene::MaterialComponent* material = scene.materials.GetComponent(groundMateral);
        material->roughness = 0.85;
        material->metalness = 0.04;

        rockMatereal = scene.CreateMaterial("TerrainRock_Material");
        material = scene.materials.GetComponent(rockMatereal);
        material->roughness = 0.95;
        material->metalness = 0.215;

        auto mergeChunks = [&](int x, int z)
        {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=](uint64_t)
            {
                Chunk chunk = { centerChunk.x + x, centerChunk.z + z };
                auto it = chunks.find(chunk);
                ChunkData& chunkData = it->second;

                scene::Scene& scene = scene::GetScene();
                scene.Merge(chunkData.scene);
                scene.ComponentAttach(chunkData.entity, terrainEntity);
            });
        };

        cancelTerrainGen.store(false);

        jobsystem::Execute(jobContext, [=](jobsystem::JobArgs)
        {
            requestChunk(0, 0);
            mergeChunks(0, 0);
            if (cancelTerrainGen.load()) 
                return;
            for (int32_t growth = 0; growth < m_meshDesc.chunkExpand; growth++)
            {
                const int side = 2 * (growth + 1);
                int x = -growth - 1;
                int z = -growth - 1;
                for (int i = 0; i < side; ++i)
                {
                    requestChunk(x, z);
                    mergeChunks(x, z);
                    if (cancelTerrainGen.load())
                        return;
                    x++;
                }
                for (int i = 0; i < side; ++i)
                {
                    requestChunk(x, z);
                    mergeChunks(x, z);
                    if (cancelTerrainGen.load())
                        return;
                    z++;
                }
                for (int i = 0; i < side; ++i)
                {
                    requestChunk(x, z);
                    mergeChunks(x, z);
                    if (cancelTerrainGen.load())
                        return;
                    x--;
                }
                for (int i = 0; i < side; ++i)
                {
                    requestChunk(x, z);
                    mergeChunks(x, z);
                    if (cancelTerrainGen.load())
                        return;
                    z--;
                }
            }
        });
    }
}