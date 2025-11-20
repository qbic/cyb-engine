#define IMGUI_DEFINE_MATH_OPERATORS
#include <algorithm>
#include <fstream>
#include "core/noise.h"
#include "core/random.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "systems/event_system.h"
#include "systems/profiler.h"
#include "editor/editor.h"
#include "editor/terrain_generator.h"
#include "editor/undo_manager.h"
#include "json.hpp"
#include "imgui_internal.h"

#define PREVIEW_RESOLUTION      128         // increase for higher quality, but slower to update preview
#define PREVIEW_FREQ_SCALE      8.0         // higher value shows more terrain, but might get noisy
#define MULTITHREADED_PREVIEW   1
#define PREVIEW_GEN_GROUP_SIZE  32

namespace cyb::editor
{
    PerlinNode::PerlinNode() :
        NG_Node(typeString, 240.0f)
    {
        AddOutputPin<noise2::NoiseNode*>("Output", [=] () { return &m_noise; });
    }

    void PerlinNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        uint32_t iTemp = m_noise.GetSeed();
        if (ui::SliderInt("Seed", (int*)&iTemp, onChange, 0, std::numeric_limits<int>::max() / 2))
            m_noise.SetSeed(iTemp);

        float fTemp = m_noise.GetFrequency();
        if (ui::SliderFloat("Frequency", &fTemp, onChange, 0.1f, 8.0f))
            m_noise.SetFrequency(fTemp);

        iTemp = m_noise.GetOctaves();
        if (ui::SliderInt("Octaves", (int*)&iTemp, onChange, 1, 6))
            m_noise.SetOctaves(iTemp);

        fTemp = m_noise.GetLacunarity();
        if (ui::SliderFloat("Lacunarity", &fTemp, onChange, 0.0f, 4.0f))
            m_noise.SetLacunarity(fTemp);

        fTemp = m_noise.GetPersistance();
        if (ui::SliderFloat("Persistance", &fTemp, onChange, 0.0f, 4.0f))
            m_noise.SetPersistence(fTemp);
    }

    CellularNode::CellularNode() :
        NG_Node(typeString, 240.0f)
    {
        AddOutputPin<noise2::NoiseNode*>("Output", [=] () { return &m_noise; });
    }

    void CellularNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        uint32_t iTemp = m_noise.GetSeed();
        if (ui::SliderInt("Seed", (int*)&iTemp, onChange, 0, std::numeric_limits<int>::max() / 2))
            m_noise.SetSeed(iTemp);

        float fTemp = m_noise.GetFrequency();
        if (ui::SliderFloat("Frequency", &fTemp, onChange, 0.1f, 8.0f))
            m_noise.SetFrequency(fTemp);

        fTemp = m_noise.GetJitterModifier();
        if (ui::SliderFloat("Jitter", &fTemp, onChange, 0.0f, 2.0f))
            m_noise.SetJitterModifier(fTemp);

        iTemp = m_noise.GetOctaves();
        if (ui::SliderInt("Octaves", (int*)&iTemp, onChange, 1, 6))
            m_noise.SetOctaves(iTemp);

        fTemp = m_noise.GetLacunarity();
        if (ui::SliderFloat("Lacunarity", &fTemp, onChange, 0.0f, 4.0f))
            m_noise.SetLacunarity(fTemp);

        fTemp = m_noise.GetPersistance();
        if (ui::SliderFloat("Persistance", &fTemp, onChange, 0.0f, 4.0f))
            m_noise.SetPersistence(fTemp);
    }

    ConstNode::ConstNode() :
        NG_Node(typeString, 160.0f)
    {
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_const; });
    }

    void ConstNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        float fTemp = m_const.GetConstValue();
        if (ui::SliderFloat("Value", &fTemp, onChange, 0.0f, 1.0f))
            m_const.SetConstValue(fTemp);
    }

    BlendNode::BlendNode() :
        NG_Node(typeString, 160.0f)
    {
        AddInputPin<noise2::NoiseNode*>("Input A", [&] (std::optional<noise2::NoiseNode*> from) {
            m_blend.SetInput(0, from.value_or(nullptr));
        });
        AddInputPin<noise2::NoiseNode*>("Input B", [&] (std::optional<noise2::NoiseNode*> from) {
            m_blend.SetInput(1, from.value_or(nullptr));
        });
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_blend; });
    }

    void BlendNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        float fTemp = m_blend.GetAlpha();
        if (ui::SliderFloat("Alpha", &fTemp, onChange, 0.0f, 1.0f))
            m_blend.SetAlpha(fTemp);
    }

    InvertNode::InvertNode() :
        NG_Node(typeString)
    {
        AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
            m_inv.SetInput(0, from.value_or(nullptr));
        });
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_inv; });
    }

    ScaleBiasNode::ScaleBiasNode() :
        NG_Node(typeString, 160.0f)
    {
        AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
            m_scaleBias.SetInput(0, from.value_or(nullptr));
        });
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_scaleBias; });
    }

    void ScaleBiasNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        float fTemp = m_scaleBias.GetScale();
        if (ui::SliderFloat("Scale", &fTemp, onChange, 0.0f, 2.0f))
            m_scaleBias.SetScale(fTemp);

        fTemp = m_scaleBias.GetBias();
        if (ui::SliderFloat("Bias", &fTemp, onChange, 0.0f, 1.0f))
            m_scaleBias.SetBias(fTemp);
    }

    StrataNode::StrataNode() :
        NG_Node(typeString, 160.0f)
    {
        AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
            m_strata.SetInput(0, from.value_or(nullptr));
        });
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_strata; });
    }

    void StrataNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        float fTemp = m_strata.GetStrata();
        if (ui::SliderFloat("Strata", &fTemp, onChange, 2.0f, 12.0f))
            m_strata.SetStrata(fTemp);
    }

    SelectNode::SelectNode() :
        NG_Node(typeString, 160.0f)
    {
        AddInputPin<noise2::NoiseNode*>("Input A", [&] (std::optional<noise2::NoiseNode*> from) {
            m_select.SetInput(0, from.value_or(nullptr));
        });
        AddInputPin<noise2::NoiseNode*>("Input B", [&] (std::optional<noise2::NoiseNode*> from) {
            m_select.SetInput(1, from.value_or(nullptr));
        });
        AddInputPin<noise2::NoiseNode*>("Control", [&] (std::optional<noise2::NoiseNode*> from) {
            m_select.SetInput(2, from.value_or(nullptr));
        });
        AddOutputPin<noise2::NoiseNode*>("Output", [&] () { return &m_select; });
    }

    void SelectNode::DisplayContent()
    {
        auto onChange = [=] () { ModifiedFlag = true; };

        float fTemp = m_select.GetThreshold();
        if (ui::SliderFloat("Threshold", &fTemp, onChange, 0.0f, 1.0f))
            m_select.SetThreshold(fTemp);

        fTemp = m_select.GetEdgeFalloff();
        if (ui::SliderFloat("Edge Falloff", &fTemp, onChange, 0.0f, 1.0f))
            m_select.SetEdgeFalloff(fTemp);
    }

    PreviewNode::PreviewNode() :
        NG_Node(typeString, 256.0f)
    {
        AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
            m_input = from.value_or(nullptr);
        });
    }

    void PreviewNode::UpdatePreview()
    {
        if (!m_input || !ValidState)
            return;

        Timer timer;

        noise2::NoiseImageDesc imageDesc{};
        imageDesc.input = m_input;
        imageDesc.size = { m_previewSize, m_previewSize };
        imageDesc.freqScale = m_freqScale / (m_previewSize / 128.0f);

        noise2::NoiseImage image{ { m_previewSize, m_previewSize } };
#if MULTITHREADED_PREVIEW
        jobsystem::Context ctx{};
        const uint32_t groupSize = PREVIEW_GEN_GROUP_SIZE;
        jobsystem::Dispatch(ctx, imageDesc.size.height, groupSize, [&] (jobsystem::JobArgs args) {
            const uint32_t rowStart = args.jobIndex;
            noise2::RenderNoiseImageRows(image, imageDesc, rowStart, 1);
        });
#else
        noise2::RenderNoiseImageRows(image, imageDesc, 0, imageDesc.size.height);
#endif

        rhi::TextureDesc desc{};
        desc.width = image.GetWidth();
        desc.height = image.GetHeight();
        desc.format = rhi::Format::RGBA8_UNORM;

        rhi::SubresourceData subresourceData;
        subresourceData.mem = image.GetConstPtr(0);
        subresourceData.rowPitch = image.GetStride();

#if MULTITHREADED_PREVIEW
        jobsystem::Wait(ctx);
#endif
        rhi::GetDevice()->CreateTexture(&desc, &subresourceData, &m_texture);
        m_lastPreviewGenerationTime = timer.ElapsedMilliseconds();
    }

    void PreviewNode::Update()
    {
        if (m_autoUpdate)
            UpdatePreview();
    }

    void PreviewNode::DisplayContent()
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        if (ui::Checkbox("Auto Update", &m_autoUpdate, nullptr) && m_autoUpdate)
            UpdatePreview();
        if (!m_autoUpdate && ImGui::Button("Update", ImVec2(0, 0)))
            UpdatePreview();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float imageWidth = window->WorkRect.GetWidth();
        const ImVec2 imageSize{ imageWidth, imageWidth };
        ImGui::Text("Size: %f", imageWidth);
        if (m_input && ValidState && m_texture.IsValid())
        {
            ImGui::Image((ImTextureID)&m_texture, imageSize);
        }
        else
        {
            const ImGuiWindow* window = ImGui::GetCurrentWindow();
            const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + imageSize);
            ImGui::ItemSize(bb);
            if (ImGui::ItemAdd(bb, ImGui::GetID(this)))
                ImGui::GetWindowDrawList()->AddRectFilled(bb.Min, bb.Max, 0xff222222);
        }

        ImGui::Text("Updated in %.2fms", m_lastPreviewGenerationTime);
        if (ui::DragInt("Preview Size", (int*)&m_previewSize, 1, 4, 1024))
            Update();
        if (ui::SliderFloat("Freq Scale", &m_freqScale, nullptr, 1.0f, 12.0f))
            Update();
    }

    GenerateMeshNode::GenerateMeshNode() :
        NG_Node(typeString)
    {
        AddInputPin<noise2::NoiseNode*>("Input", [&] (std::optional<noise2::NoiseNode*> from) {
            m_input = from.value_or(nullptr);
        });

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
    }

    void GenerateMeshNode::DisplayContent()
    {
        ui::SliderInt("ChunkExpand", (int*)&m_chunkExpand, nullptr, 0, 24);
        ui::DragInt("ChunkSize", &m_chunkSize, 1, 1, 2000, "%dm");
        ui::DragFloat("Min Altitude", &m_minMeshAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
        ui::DragFloat("Max Altitude", &m_maxMeshAltitude, 0.5f, -500.0f, 500.0f, "%.2fm");
        ui::SliderFloat("Max Error", &m_maxError, nullptr, 0.0001f, 0.01f, "%.4f");
        //ui::GradientButton("ColorBand", &m_biomeColorBand);
        
        ImGui::Spacing();
        ImGui::Spacing();

        if (!jobsystem::IsBusy(m_jobContext))
        {
            if (ImGui::Button("Generate Mesh", ImVec2(-1.0f, 0.0f)))
            {
                //GenerateTerrainMesh();
            }
        }
        else
        {
            if (ImGui::Button("[Cancel Generation]", ImVec2(0.0f, 0.0f)))
            {
                //cancelTerrainGen.store(true);
            }
        }
    }

    // Colorize vertical faces with rock color.
    // Indices are re-ordered, first are the ground indices, then the rock indices.
    // Returns the number of ground indices (offset for rock indices)
    uint32_t ColorizeMountains(scene::MeshComponent* mesh)
    {
        const uint32_t rockColor = StoreColor_RGBA(XMFLOAT4{ 0.6, 0.6, 0.6, 1 });

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

    void GenerateMeshNode::GenerateTerrainMesh()
    {
        // Remove any previous data from the scene
        scene::Scene& scene = scene::GetScene();
        scene.RemoveEntity(m_terrainGroupID, true, true);

        std::unordered_map<Chunk, ChunkData, ChunkHash> chunks;
        Chunk centerChunk = {};

        // create terrain group entity
        m_terrainGroupID = scene.CreateGroup("Terrain");

        // create terrain materials
        ecs::Entity groundMateralID = scene.CreateMaterial("TerrainGround_Material");
        scene::MaterialComponent* material = scene.materials.GetComponent(groundMateralID);
        material->roughness = 0.85;
        material->metalness = 0.04;

        ecs::Entity rockMaterealID = scene.CreateMaterial("TerrainRock_Material");
        material = scene.materials.GetComponent(rockMaterealID);
        material->roughness = 0.95;
        material->metalness = 0.215;

        auto requestChunk = [&] (int32_t xOffset, int32_t zOffset) {
            Chunk chunk = { centerChunk.x + xOffset, centerChunk.z + zOffset };
            auto it = chunks.find(chunk);
            if (it != chunks.end() && it->second.entity != ecs::INVALID_ENTITY)
                return;

            // generate a new chunk
            ChunkData& chunkData = chunks[chunk];
            scene::Scene& chunkScene = chunkData.scene;
            chunkData.entity = chunkScene.CreateObject(std::format("Chunk_{}_{}", xOffset, zOffset));
            scene::ObjectComponent* object = chunkScene.objects.GetComponent(chunkData.entity);

            scene::TransformComponent* transform = chunkScene.transforms.GetComponent(chunkData.entity);
            transform->Translate(XMFLOAT3((float)(xOffset * m_chunkSize), 0, static_cast<float>(zOffset * m_chunkSize)));
            transform->UpdateTransform();

            scene::MeshComponent* mesh = &chunkScene.meshes.Create(chunkData.entity);
            object->meshID = chunkData.entity;

            // generate triangulated heightmap mesh
            XMINT2 heightmapOffset = XMINT2(xOffset * m_chunkSize, zOffset * m_chunkSize);
            //HeightmapTriangulator triangulator(&heightmap, m_meshDesc.size, m_meshDesc.size, heightmapOffset);
            //triangulator.Run(m_meshDesc.maxError, m_meshDesc.maxVertices, m_meshDesc.maxTriangles);

            jobsystem::Context ctx;
            ctx.allowWorkOnMainThread = false;

            std::vector<XMFLOAT3> points;
            jobsystem::Execute(ctx, [&] (jobsystem::JobArgs args) {
                //points = triangulator.GetPoints();
            });

            std::vector<XMINT3> triangles;
            jobsystem::Execute(ctx, [&] (jobsystem::JobArgs args) {
                //triangles = triangulator.GetTriangles();
            });

            jobsystem::Wait(ctx);

            // load mesh vertices
            const XMFLOAT3 offsetToCenter = XMFLOAT3(-(m_chunkSize * 0.5f), 0.0f, -(m_chunkSize * 0.5f));
            mesh->vertex_positions.resize(points.size());
            mesh->vertex_colors.resize(points.size());
            jobsystem::Dispatch(ctx, (uint32_t)points.size(), 512, [&] (jobsystem::JobArgs args) {
                const uint32_t index = args.jobIndex;
                mesh->vertex_positions[index] = XMFLOAT3(
                    offsetToCenter.x + points[index].x * m_chunkSize,
                    offsetToCenter.y + Lerp(m_minMeshAltitude, m_maxMeshAltitude, points[index].y),
                    offsetToCenter.z + points[index].z * m_chunkSize);
                mesh->vertex_colors[index] = m_biomeColorBand.GetColorAt(points[index].y);
            });

            // load mesh indexes
            mesh->indices.resize(triangles.size() * 3);
            jobsystem::Dispatch(ctx, (uint32_t)triangles.size(), 512, [&] (jobsystem::JobArgs args) {
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
            subset.materialID = groundMateralID;
            mesh->subsets.push_back(subset);

            // setup subset using rock material
            subset.indexOffset = groundIndices;
            subset.indexCount = (uint32_t)mesh->indices.size() - groundIndices;
            subset.materialID = rockMaterealID;
            mesh->subsets.push_back(subset);

            mesh->ComputeSmoothNormals();
            mesh->CreateRenderData();
        };

        auto mergeChunks = [&] (int x, int z) {
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [&] (uint64_t) {
                Chunk chunk = { centerChunk.x + x, centerChunk.z + z };
                auto it = chunks.find(chunk);
                ChunkData& chunkData = it->second;

                scene::Scene& scene = scene::GetScene();
                scene.Merge(chunkData.scene);
                scene.ComponentAttach(chunkData.entity, m_terrainGroupID);
            });
        };

        std::atomic_bool cancelTerrainGen{ false };

        m_jobContext.allowWorkOnMainThread = false;
        jobsystem::Execute(m_jobContext, [&] (jobsystem::JobArgs) {
            requestChunk(0, 0);
            mergeChunks(0, 0);
            if (cancelTerrainGen.load())
                return;

            for (int32_t growth = 0; growth < m_chunkExpand; growth++)
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

    NoiseNode_Factory::NoiseNode_Factory()
    {
        // Producer node types
        RegisterNodeType<CellularNode>(CellularNode::typeString, Category::Producer);
        RegisterNodeType<ConstNode>(ConstNode::typeString, Category::Producer);
        RegisterNodeType<PerlinNode>(PerlinNode::typeString, Category::Producer);

        // Modifier node types
        RegisterNodeType<BlendNode>(BlendNode::typeString, Category::Modifier);
        RegisterNodeType<InvertNode>(InvertNode::typeString, Category::Modifier);
        RegisterNodeType<ScaleBiasNode>(ScaleBiasNode::typeString, Category::Modifier);
        RegisterNodeType<SelectNode>(SelectNode::typeString, Category::Modifier);
        RegisterNodeType<StrataNode>(StrataNode::typeString, Category::Modifier);

        // Consumer node types
        RegisterNodeType<GenerateMeshNode>(GenerateMeshNode::typeString, Category::Consumer);
        RegisterNodeType<PreviewNode>(PreviewNode::typeString, Category::Consumer);
    }

    void NoiseNode_Factory::DrawMenuContent(ui::NG_Canvas& canvas, const ImVec2& popupPos)
    {
        const size_t total = m_categories.size();
        size_t count = 0;

        for (auto& [category, types] : m_categories)
        {
            for (auto& type : types)
            {
                if (ImGui::MenuItem(type.c_str()))
                    canvas.Nodes.push_back(std::move(CreateNode(type, popupPos)));
            }

            // Skip the seperator the the last category.
            if (++count < total)
                ImGui::Separator();
        }
    }


    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////


    static const std::unordered_map<HeightmapStrataOp, std::string> g_strataFuncCombo = {
        { HeightmapStrataOp::None,              "None"          },
        { HeightmapStrataOp::SharpSub,          "SharpSub"      },
        { HeightmapStrataOp::SharpAdd,          "SharpAdd"      },
        { HeightmapStrataOp::Quantize,          "Quantize"      },
        { HeightmapStrataOp::Smooth,            "Smooth"        }
    };

    static const std::unordered_map<noise::Type, std::string> g_noiseTypeCombo = {
        { noise::Type::Perlin,                  "Perlin"        },
        { noise::Type::Cellular,                "Cellular"      }
    };

    static const std::unordered_map<HeightmapCombineOp, std::string> g_mixOpCombo = {
        { HeightmapCombineOp::Add,              "Add"           },
        { HeightmapCombineOp::Sub,              "Sub"           },
        { HeightmapCombineOp::Mul,              "Mul"           },
        { HeightmapCombineOp::Lerp,             "Lerp"          }
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
        UpdatePreview();
    }

    void TerrainGenerator::SetDefaultHeightmapValues()
    {
        heightmap.inputList = GetDefaultInputs();
    }

    void TerrainGenerator::DrawGui(ecs::Entity selectedEntity)
    {
        static const uint32_t settingsPandelWidth = 340;
#if 0
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Settings", ""))
                {
                    filesystem::SaveDialog("JSON (*.json)\0*.json\0", [&](std::string filename) {
                        nlohmann::ordered_json json;
                        auto& terrain = json["terrain_settings"];
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

                        UpdatePreview();
                        ui::GetUndoManager().ClearHistory();
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
#endif

        const auto updatePreviewCb = std::bind(&TerrainGenerator::UpdatePreview, this);

        if (ImGui::BeginTable("TerrainGenerator", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_NoClip);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);

            // begin drawing of the settings panel
            ImGui::TableNextColumn();
            ImGui::BeginChild("Settings panel", ImVec2(settingsPandelWidth, 680), ImGuiChildFlags_Border);
            ImGui::PushItemWidth(-1);

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

                    const ui::PushID idGuard((void*)&input);
                    ui::ComboBox("NoiseType", input.noise.type, g_noiseTypeCombo, updatePreviewCb);
                    if (input.noise.type == noise::Type::Cellular)
                        ui::ComboBox("CelReturn", input.noise.cellularReturnType, g_cellularReturnTypeCombo, updatePreviewCb);

                    ui::SliderInt("Seed", (int*)&input.noise.seed, updatePreviewCb, 0, std::numeric_limits<int>::max() / 2);
                    ui::SliderFloat("Frequency", &input.noise.frequency, updatePreviewCb, 0.0f, 10.0f);
                    ui::ComboBox("HeightmapStrataOp", input.strataOp, g_strataFuncCombo, updatePreviewCb);
                    if (input.strataOp != HeightmapStrataOp::None)
                        ui::SliderFloat("Strata", &input.strata, updatePreviewCb, 1.0f, 15.0f);
                    ui::SliderFloat("Exponent", &input.exponent, updatePreviewCb, 0.0f, 4.0f);

                    if (ImGui::TreeNode("FBM"))
                    {
                        ui::SliderInt("Octaves", (int*)&input.noise.octaves, updatePreviewCb, 1, 8);
                        ui::SliderFloat("Lacunarity", &input.noise.lacunarity, updatePreviewCb, 0.0f, 4.0f);
                        ui::SliderFloat("Gain", &input.noise.gain, updatePreviewCb, 0.0f, 4.0f);
                        ImGui::TreePop();
                    }

                    // only show combine options if there is an additional input after
                    if (i + 1 < heightmap.inputList.size())
                    {
                        ui::ComboBox("HeightmapCombineOp", input.combineOp, g_mixOpCombo, updatePreviewCb);
                        if (input.combineOp == HeightmapCombineOp::Lerp)
                            ui::SliderFloat("Mix", &input.mixing, updatePreviewCb, 0.0f, 1.0f);
                    }

                    ImGui::Separator();
                }
            }

            //ImGui::Spacing();
            //if (ui::GradientButton("ColorBand", &m_biomeColorBand))
            //    UpdatePreview();

            ImGui::PopItemWidth();
            ImGui::EndChild();
            ImGui::BeginChild("Settings buttons", ImVec2(settingsPandelWidth, 130), ImGuiChildFlags_Border);

            if (ImGui::Button("Set Default Params", ImVec2(-1, 0)))
            {
                SetDefaultHeightmapValues();
                UpdatePreview();
            }

            if (ImGui::Button("Random seed", ImVec2(-1, 0)))
            {
                for (auto& input : heightmap.inputList)
                    input.noise.seed = random::GetNumber<uint32_t>(0, std::numeric_limits<int>::max());

                UpdatePreview();
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
            if (m_preview.IsValid())
            {
                ImGui::PushItemWidth(300);
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const float size = Max(200.0f, Min(available.x - 4, available.y - 4));
                const ImVec2 p0 = ImGui::GetCursorScreenPos();
                const ImVec2 p1 = ImVec2(p0.x + size, p0.y + size);

                ImGui::InvisibleButton("##HeightPreviewImage", ImVec2(size, size));
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddImage((ImTextureID)&m_preview, p0, p1);
                const std::string info = std::format("offset ({}, {}), use left mouse button to drag", m_previewOffset.x, m_previewOffset.y);
                drawList->AddText(ImVec2(p0.x + 8, p0.y + 8), 0xFFFFFFFF, info.c_str());

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ImGuiIO& io = ImGui::GetIO();
                    m_previewOffset.x -= static_cast<int>(io.MouseDelta.x);
                    m_previewOffset.y -= static_cast<int>(io.MouseDelta.y);
                    updatePreviewCb();
                }
                ImGui::PopItemWidth();
            }

            ImGui::EndTable();
        }
    }

    // generate preview texture aswell as generating some min
    // max values in heightmap generator for psuedo normalization
    void TerrainGenerator::UpdatePreview() {
        jobsystem::Wait(m_updatePreviewCtx);
       // jobsystem::Execute(m_updatePreviewCtx, [&](jobsystem::JobArgs) {
#if 0
            std::vector<float> image;

            heightmap.UnlockMinMax();
            CreateHeightmapImage(image, PREVIEW_RESOLUTION, PREVIEW_RESOLUTION, heightmap, m_previewOffset.x, m_previewOffset.y, PREVIEW_FREQ_SCALE);
            heightmap.LockMinMax();
            //CYB_TRACE("MIN={} MAX={}", heightmap.minHeight, heightmap.maxHeight);

            // normalize preview image values to 0.0f - 1.0f range
            const float invHeight = 1.0f / (heightmap.GetMaxHeight() - heightmap.GetMinHeight());
            for (uint32_t i = 0; i < PREVIEW_RESOLUTION * PREVIEW_RESOLUTION; ++i) {
                image[i] = (image[i] - heightmap.GetMinHeight()) * invHeight;
            }

            // create a one channel float texture with all components
            // swizzled for easy grayscale viewing
            rhi::TextureDesc desc{};
            desc.width = PREVIEW_RESOLUTION;
            desc.height = PREVIEW_RESOLUTION;
            desc.format = rhi::Format::R32_FLOAT;
            desc.swizzle.r = rhi::ComponentSwizzle::R;
            desc.swizzle.g = rhi::ComponentSwizzle::R;
            desc.swizzle.b = rhi::ComponentSwizzle::R;

            const rhi::FormatInfo& formatInfo = rhi::GetFormatInfo(desc.format);
            rhi::SubresourceData subresourceData;
            subresourceData.mem      = image.data();
            subresourceData.rowPitch = desc.width * formatInfo.bytesPerBlock;

            rhi::GetDevice()->CreateTexture(&desc, &subresourceData, &m_preview);
#else
        
            {
                auto ground = noise2::NoiseNode_Perlin()
                    .SetSeed(33)
                    .SetFrequency(0.8)
                    .SetOctaves(3);
                auto groundScaled = noise2::NoiseNode_ScaleBias()
                    .SetInput(0, &ground)
                    .SetScale(0.225);

                auto mountain = noise2::NoiseNode_Perlin()
                    .SetSeed(heightmap.inputList[0].noise.seed)
                    .SetFrequency(heightmap.inputList[0].noise.frequency)
                    .SetOctaves(heightmap.inputList[0].noise.octaves);
                auto mountainStrat = noise2::NoiseNode_Strata()
                    .SetInput(0, &mountain)
                    .SetStrata(6);

                auto control = noise2::NoiseNode_Perlin()
                    .SetSeed(553)
                    .SetFrequency(0.7)
                    .SetOctaves(4)
                    .SetPersistence(0.25);

                auto finalTerrain = noise2::NoiseNode_Select()
                    .SetInput(0, &groundScaled)
                    .SetInput(1, &mountainStrat)
                    .SetInput(2, &control)
                    .SetThreshold(0.72)
                    .SetEdgeFalloff(0.125);

                noise2::NoiseImageDesc imageDesc{};
                imageDesc.input = &finalTerrain;
                imageDesc.size = { PREVIEW_RESOLUTION, PREVIEW_RESOLUTION };
                imageDesc.offset = { m_previewOffset.x, m_previewOffset.y };
                imageDesc.freqScale = PREVIEW_FREQ_SCALE;
                auto image = RenderNoiseImage(imageDesc);

                rhi::TextureDesc desc{};
                desc.width = PREVIEW_RESOLUTION;
                desc.height = PREVIEW_RESOLUTION;
                desc.format = rhi::Format::RGBA8_UNORM;

                const rhi::FormatInfo& formatInfo = rhi::GetFormatInfo(desc.format);
                rhi::SubresourceData subresourceData;
                subresourceData.mem = image->GetConstPtr(0);
                subresourceData.rowPitch = desc.width * formatInfo.bytesPerBlock;

                rhi::GetDevice()->CreateTexture(&desc, &subresourceData, &m_preview);
            }
#endif
        //});
    }

    
    void TerrainGenerator::RemoveTerrainData()
    {
        scene::Scene& scene = scene::GetScene();
        chunks.clear();
        scene.RemoveEntity(terrainEntity, true, true);
    }

    void TerrainGenerator::GenerateTerrainMesh()
    {
        RemoveTerrainData();

        auto requestChunk = [&](int32_t xOffset, int32_t zOffset) {
            Chunk chunk = { centerChunk.x + xOffset, centerChunk.z + zOffset };
            auto it = chunks.find(chunk);
            if (it != chunks.end() && it->second.entity != ecs::INVALID_ENTITY)
                return;

            // generate a new chunk
            ChunkData& chunkData = chunks[chunk];
            scene::Scene& chunkScene = chunkData.scene;
            chunkData.entity = chunkScene.CreateObject(std::format("Chunk_{}_{}", xOffset, zOffset));
            scene::ObjectComponent* object = chunkScene.objects.GetComponent(chunkData.entity);

            scene::TransformComponent* transform = chunkScene.transforms.GetComponent(chunkData.entity);
            transform->Translate(XMFLOAT3((float)(xOffset * m_meshDesc.size), 0, static_cast<float>(zOffset * m_meshDesc.size)));
            transform->UpdateTransform();

            scene::MeshComponent* mesh = &chunkScene.meshes.Create(chunkData.entity);
            object->meshID = chunkData.entity;

            // generate triangulated heightmap mesh
            XMINT2 heightmapOffset = XMINT2(xOffset * m_meshDesc.size, zOffset * m_meshDesc.size);
            HeightmapTriangulator triangulator(&heightmap, m_meshDesc.size, m_meshDesc.size, heightmapOffset);
            triangulator.Run(m_meshDesc.maxError, m_meshDesc.maxVertices, m_meshDesc.maxTriangles);

            jobsystem::Context ctx;
            ctx.allowWorkOnMainThread = false;

            std::vector<XMFLOAT3> points;
            jobsystem::Execute(ctx, [&](jobsystem::JobArgs args) {
                points = triangulator.GetPoints();
            });

            std::vector<XMINT3> triangles;
            jobsystem::Execute(ctx, [&](jobsystem::JobArgs args) {
                triangles = triangulator.GetTriangles();
            });

            jobsystem::Wait(ctx);

            // load mesh vertices
            const XMFLOAT3 offsetToCenter = XMFLOAT3(-(m_meshDesc.size * 0.5f), 0.0f, -(m_meshDesc.size * 0.5f));
            mesh->vertex_positions.resize(points.size());
            mesh->vertex_colors.resize(points.size());
            jobsystem::Dispatch(ctx, (uint32_t)points.size(), 512, [&](jobsystem::JobArgs args) {
                const uint32_t index = args.jobIndex;
                mesh->vertex_positions[index] = XMFLOAT3(
                    offsetToCenter.x + points[index].x * m_meshDesc.size,
                    offsetToCenter.y + Lerp(m_meshDesc.minAltitude, m_meshDesc.maxAltitude, points[index].y),
                    offsetToCenter.z + points[index].z * m_meshDesc.size);
                mesh->vertex_colors[index] = m_biomeColorBand.GetColorAt(points[index].y);
            });

            // load mesh indexes
            mesh->indices.resize(triangles.size() * 3);
            jobsystem::Dispatch(ctx, (uint32_t)triangles.size(), 512, [&](jobsystem::JobArgs args) {
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

            mesh->ComputeSmoothNormals();
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
            eventsystem::Subscribe_Once(eventsystem::Event_ThreadSafePoint, [=](uint64_t) {
                Chunk chunk = { centerChunk.x + x, centerChunk.z + z };
                auto it = chunks.find(chunk);
                ChunkData& chunkData = it->second;

                scene::Scene& scene = scene::GetScene();
                scene.Merge(chunkData.scene);
                scene.ComponentAttach(chunkData.entity, terrainEntity);
            });
        };

        cancelTerrainGen.store(false);

        jobContext.allowWorkOnMainThread = false;
        jobsystem::Execute(jobContext, [=](jobsystem::JobArgs) {
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