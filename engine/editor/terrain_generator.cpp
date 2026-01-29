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
#include "editor/heightmap.h"
#include "editor/terrain_generator.h"
#include "editor/undo_manager.h"
#include "json.hpp"
#include "imgui_internal.h"

#define MULTITHREADED_PREVIEW   1
#define PREVIEW_GEN_GROUP_SIZE  32          // rows per thread group when multithreading

namespace cyb::editor
{
    PerlinNode::PerlinNode() :
        NG_Node(type_string, 240.0f)
    {
        AddOutputPin<Signature>("Output", [&] (float x, float y) -> float { 
            return noise2::PerlinNoise2D_FBM(m_param, x, y);
        });
    }

    void PerlinNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderInt("Seed", (int*)&m_param.seed, onChange, 0, std::numeric_limits<int>::max() / 2);
        ui::SliderFloat("Frequency", &m_param.frequency, onChange, 0.1f, 8.0f);
        ui::SliderInt("Octaves", (int*)&m_param.octaves, onChange, 1, 6);
        ui::SliderFloat("Lacunarity", &m_param.lacunarity, onChange, 0.0f, 4.0f);
        ui::SliderFloat("Persistence", &m_param.persistence, onChange, 0.0f, 4.0f);
    }

    void PerlinNode::SerializeToJson(json_type& json) const
    {
        json["seed"] = m_param.seed;
        json["frequency"] = m_param.frequency;
        json["octaves"] = m_param.octaves;
        json["lacunarity"] = m_param.lacunarity;
        json["persistence"] = m_param.persistence;
    }

    void PerlinNode::SerializeFromJson(const json_type& json)
    {
        m_param.seed = json.value("seed", m_param.seed);
        m_param.frequency = json.value("frequency", m_param.frequency);
        m_param.octaves = json.value("octaves", m_param.octaves);
        m_param.lacunarity = json.value("lacunarity", m_param.lacunarity);
        m_param.persistence = json.value("persistence", m_param.persistence);
    }

    CellularNode::CellularNode() :
        NG_Node(type_string, 240.0f)
    {
        AddOutputPin<Signature>("Output", [&] (float x, float y) -> float {
            return noise2::CellularNoise2D_FBM(m_param, x, y);
        });
    }

    void CellularNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderInt("Seed", (int*)&m_param.seed, onChange, 0, std::numeric_limits<int>::max() / 2);
        ui::SliderFloat("Frequency", &m_param.frequency, onChange, 0.1f, 8.0f);
        ui::SliderFloat("Jitter", &m_param.jitterModifier, onChange, 0.0f, 2.0f);
        ui::SliderInt("Octaves", (int*)&m_param.octaves, onChange, 1, 6);
        ui::SliderFloat("Lacunarity", &m_param.lacunarity, onChange, 0.0f, 4.0f);
        ui::SliderFloat("Persistence", &m_param.persistence, onChange, 0.0f, 4.0f);
    }

    void CellularNode::SerializeToJson(json_type& json) const
    {
        json["seed"] = m_param.seed;
        json["frequency"] = m_param.frequency;
        json["jitter_modifier"] = m_param.jitterModifier;
        json["octaves"] = m_param.octaves;
        json["lacunarity"] = m_param.lacunarity;
        json["persistence"] = m_param.persistence;
    }

    void CellularNode::SerializeFromJson(const json_type& json)
    {
        m_param.seed = json["seed"];
        m_param.frequency = json["frequency"];
        m_param.jitterModifier = json["jitter_modifier"];
        m_param.octaves = json["octaves"];
        m_param.lacunarity = json["lacunarity"];
        m_param.persistence = json["persistence"];
    }

    ConstNode::ConstNode() :
        NG_Node(type_string, 160.0f)
    {
        AddOutputPin<float(float, float)>("Output", [&] (float x, float y) -> float {
            return m_value;
        });
    }

    void ConstNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderFloat("Value", &m_value, onChange, 0.0f, 1.0f);
    }

    void ConstNode::SerializeToJson(json_type& json) const
    {
        json["value"] = m_value;
    }

    void ConstNode::SerializeFromJson(const json_type& json)
    {
        m_value = json["value"];
    }

    BlendNode::BlendNode() :
        NG_Node(type_string, 160.0f)
    {
        m_inputA = AddInputPin<Signature>("Input A");
        m_inputB = AddInputPin<Signature>("Input B");
        AddOutputPin<Signature>("Lerp", [&] (float x, float y) -> float {
            const float valueA = m_inputA->Invoke(x, y);
            const float valueB = m_inputB->Invoke(x, y);
            return Lerp(valueA, valueB, m_alpha);
        });
        AddOutputPin<Signature>("Mul", [&] (float x, float y) -> float {
            const float valueA = m_inputA->Invoke(x, y);
            const float valueB = m_inputB->Invoke(x, y);
            return valueA * valueB;
        });
        AddOutputPin<Signature>("Add", [&] (float x, float y) -> float {
            const float valueA = m_inputA->Invoke(x, y);
            const float valueB = m_inputB->Invoke(x, y);
            return valueA + valueB;
        });
    }

    void BlendNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderFloat("Alpha", &m_alpha, onChange, 0.0f, 1.0f);
    }

    void BlendNode::SerializeToJson(json_type& json) const
    {
        json["alpha"] = m_alpha;
    }

    void BlendNode::SerializeFromJson(const json_type& json)
    {
        m_alpha = json["alpha"];
    }

    InvertNode::InvertNode() :
        NG_Node(type_string)
    {
        m_input = AddInputPin<Signature>("Input");
        AddOutputPin<Signature>("Output", [&] (float x, float y) {
            return 1.0f - m_input->Invoke(x, y); 
        });
    }

    ScaleBiasNode::ScaleBiasNode() :
        NG_Node(type_string, 160.0f)
    {
        m_input = AddInputPin<Signature>("Input");
        AddOutputPin<Signature>("Output", [&] (float x, float y) -> float {
            return m_input->Invoke(x, y) * m_scale + m_bias;
        });
    }

    void ScaleBiasNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderFloat("Scale", &m_scale, onChange, 0.0f, 2.0f);
        ui::SliderFloat("Bias", &m_bias, onChange, 0.0f, 1.0f);
    }

    void ScaleBiasNode::SerializeToJson(json_type& json) const
    {
        json["scale"] = m_scale;
        json["bias"] = m_bias;
    }

    void ScaleBiasNode::SerializeFromJson(const json_type& json)
    {
        m_scale = json["scale"];
        m_bias = json["bias"];
    }

    StrataNode::StrataNode() :
        NG_Node(type_string, 160.0f)
    {
        m_input = AddInputPin<Signature>("Input");
        AddOutputPin<Signature>("Output", [&] (float x, float y) -> float {
            const float value = m_input->Invoke(x, y);
            const float step = std::floor(value * m_strata) / m_strata;
            const float alpha = CubicSmoothStep((value * m_strata) - std::floor(value * m_strata));
            return Lerp(step, value, alpha);
        });
    }

    void StrataNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderFloat("Strata", &m_strata, onChange, 2.0f, 12.0f);
    }

    void StrataNode::SerializeToJson(json_type& json) const
    {
        json["strata"] = m_strata;
    }

    void StrataNode::SerializeFromJson(const json_type& json)
    {
        m_strata = json["strata"];
    }

    SelectNode::SelectNode() :
        NG_Node(type_string, 160.0f)
    {
        m_inputA = AddInputPin<Signature>("Input A");
        m_inputB = AddInputPin<Signature>("Input B");
        m_inputControl = AddInputPin<Signature>("Control");
        AddOutputPin<Signature>("Output", [&] (float x, float y) -> float {
            const float valueA = m_inputA->Invoke(x, y);
            const float valueB = m_inputB->Invoke(x, y);
            const float controlValue = m_inputControl->Invoke(x, y);

            if (m_edgeFalloff > 0.0f)
            {
                const float lower = m_threshold - m_edgeFalloff;
                const float upper = m_threshold + m_edgeFalloff;

                if (controlValue < lower)
                    return valueA;
                else if (controlValue > upper)
                    return valueB;
                else
                {
                    const float alpha = CubicSmoothStep((controlValue - lower) / (2.0f * m_edgeFalloff));
                    return Lerp(valueA, valueB, alpha);
                }
            }

            return  (controlValue < m_threshold) ? valueA : valueB;
        });
    }

    void SelectNode::DisplayContent()
    {
        auto onChange = [&] () { ModifiedFlag = true; };

        ui::SliderFloat("Threshold", &m_threshold, onChange, 0.0f, 1.0f);
        ui::SliderFloat("Edge Falloff", &m_edgeFalloff, onChange, 0.0f, 1.0f);
    }

    void SelectNode::SerializeToJson(json_type& json) const
    {
        json["threshold"] = m_threshold;
        json["edge_falloff"] = m_edgeFalloff;
    }

    void SelectNode::SerializeFromJson(const json_type& json)
    {
        m_threshold = json["threshold"];
        m_edgeFalloff = json["edge_falloff"];
    }

    struct NoiseNodeImageDesc : noise2::NoiseImageDesc
    {
        ImageGenInputPin inputPin;

        float GetValue(float x, float y) const override
        {
            if (!inputPin)
                return 0.0f;
            float v = inputPin->Invoke(x, y);
            return v;
        }
    };

    PreviewNode::PreviewNode() :
        NG_Node(type_string, 256.0f)
    {
        m_inputPin = AddInputPin<Signature>("Input");
        AddOutputPin<Signature>("Passthrough", [&] (float x, float y) -> float {
            return m_inputPin->Invoke(x, y);
        });
    }

    void PreviewNode::UpdatePreview()
    {
        if (!ValidState)
            return;

        Timer timer;

        NoiseNodeImageDesc imageDesc{};
        imageDesc.inputPin = m_inputPin;
        imageDesc.size = { m_previewSize, m_previewSize };
        imageDesc.freqScale = m_freqScale / (m_previewSize / 128.0f);

        noise2::NoiseImage image{ { m_previewSize, m_previewSize } };
#if MULTITHREADED_PREVIEW
        jobsystem::Context ctx{};
        const uint32_t groupSize = PREVIEW_GEN_GROUP_SIZE;
        jobsystem::Dispatch(ctx, imageDesc.size.height, groupSize, [&] (jobsystem::JobArgs args) {
            const uint32_t rowStart = args.jobIndex;
            noise2::RenderNoiseImageRows(image, &imageDesc, rowStart, 1);
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
        if (ValidState && m_texture.IsValid())
        {
            ImGui::Image((ImTextureID)&m_texture, imageSize);
        }
        else
        {
            // Display a gray box when no image is available
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

    void PreviewNode::SerializeToJson(json_type& json) const
    {
        json["auto_update"] = m_autoUpdate;
        json["preview_size"] = m_previewSize;
        json["freq_scale"] = m_freqScale;
    }

    void PreviewNode::SerializeFromJson(const json_type& json)
    {
        m_autoUpdate = json["auto_update"];
        m_previewSize = json["preview_size"];
        m_freqScale = json["freq_scale"];
    }

    GenerateMeshNode::GenerateMeshNode() :
        NG_Node(type_string)
    {
        m_input = AddInputPin<Signature>("Input");

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
        ui::DragInt("ChunkSize", (int*)&m_chunkSize, 1, 1, 2000, "%dm");
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
                GenerateTerrainMesh();
            }
        }
        else
        {
            if (ImGui::Button("[Cancel Generation]", ImVec2(0.0f, 0.0f)))
            {
                //cancelTerrainGen.store(true);
            }
        }

        if (m_generationTime > 0.0f)
            ImGui::Text("Generated in %.2fms", m_generationTime);
    }

    void GenerateMeshNode::SerializeToJson(json_type& json) const
    {
        json["chunk_expand"] = m_chunkExpand;
        json["chunk_size"] = m_chunkSize;
        json["max_error"] = m_maxError;
        json["min_mesh_altitude"] = m_minMeshAltitude;
        json["max_mesh_altitude"] = m_maxMeshAltitude;
        //json["biome_color_band"] = m_biomeColorBand;
    }

    void GenerateMeshNode::SerializeFromJson(const json_type& json)
    {
        m_chunkExpand = json["chunk_expand"];
        m_chunkSize = json["chunk_size"];
        m_maxError = json["max_error"];
        m_minMeshAltitude = json["min_mesh_altitude"];
        m_maxMeshAltitude = json["max_mesh_altitude"];
        //m_biomeColorBand = json["biome_color_band"];
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

        // Loop though all indices and separate ground from rock indices
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

        Timer timer;

        std::unordered_map<Chunk, ChunkData, ChunkHash> chunks;
        Chunk centerChunk = {};

        // create terrain group entity
        m_terrainGroupID = scene.CreateGroup("Terrain");

        // create terrain materials
        ecs::Entity groundMaterialID = scene.CreateMaterial("TerrainGround_Material");
        scene::MaterialComponent* material = scene.materials.GetComponent(groundMaterialID);
        material->roughness = 0.85;
        material->metalness = 0.04;

        ecs::Entity rockMaterialID = scene.CreateMaterial("TerrainRock_Material");
        material = scene.materials.GetComponent(rockMaterialID);
        material->roughness = 0.95;
        material->metalness = 0.215;

        // Setup noise image descriptor
        NoiseNodeImageDesc heightmap{};
        heightmap.inputPin = m_input;
        heightmap.size = { 512 , 512 };

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
            const XMINT2 heightmapOffset{ xOffset * int32_t(m_chunkSize), zOffset * int32_t(m_chunkSize) };
            const Heightmap hm{ &heightmap, m_chunkSize, m_chunkSize, heightmapOffset };
            DelaunayTriangulator triangulator{ hm, m_chunkSize, m_chunkSize };
            triangulator.Triangulate(m_maxError, 0, 0);

            jobsystem::Context ctx;
            ctx.allowWorkOnMainThread = false;

            std::vector<XMFLOAT3> points;
            jobsystem::Execute(ctx, [&] (jobsystem::JobArgs args) {
                points = triangulator.GetPoints();
            });

            std::vector<XMINT3> triangles;
            jobsystem::Execute(ctx, [&] (jobsystem::JobArgs args) {
                triangles = triangulator.GetTriangles();
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

            // separate rock surfaces from ground to enable them
            // to have different materials
            jobsystem::Wait(ctx);
            uint32_t groundIndices = ColorizeMountains(mesh);

            // setup a subset using ground material
            scene::MeshComponent::MeshSubset subset;
            subset.indexOffset = 0;
            subset.indexCount = (uint32_t)groundIndices;
            subset.materialID = groundMaterialID;
            mesh->subsets.push_back(subset);

            // setup subset using rock material
            subset.indexOffset = groundIndices;
            subset.indexCount = (uint32_t)mesh->indices.size() - groundIndices;
            subset.materialID = rockMaterialID;
            mesh->subsets.push_back(subset);

            mesh->ComputeSmoothNormals();
            mesh->CreateRenderData();
        };

        auto mergeChunks = [&] (int x, int z) {
            Chunk chunk = { centerChunk.x + x, centerChunk.z + z };
            auto it = chunks.find(chunk);
            ChunkData& chunkData = it->second;

            scene::Scene& scene = scene::GetScene();
            scene.Merge(chunkData.scene);
            scene.ComponentAttach(chunkData.entity, m_terrainGroupID);
        };

        requestChunk(0, 0);
        mergeChunks(0, 0);

        for (int32_t growth = 0; growth < m_chunkExpand; growth++)
        {
            const int side = 2 * (growth + 1);
            int x = -growth - 1;
            int z = -growth - 1;
            for (int i = 0; i < side; ++i)
            {
                requestChunk(x, z);
                mergeChunks(x, z);
                x++;
            }
            for (int i = 0; i < side; ++i)
            {
                requestChunk(x, z);
                mergeChunks(x, z);
                z++;
            }
            for (int i = 0; i < side; ++i)
            {
                requestChunk(x, z);
                mergeChunks(x, z);
                x--;
            }
            for (int i = 0; i < side; ++i)
            {
                requestChunk(x, z);
                mergeChunks(x, z);
                z--;
            }
        }

        m_generationTime = timer.ElapsedMilliseconds();
    }

    NoiseNode_Factory::NoiseNode_Factory()
    {
        // Producer node types
        RegisterNodeType<CellularNode>(CellularNode::type_string, Category::Producer);
        RegisterNodeType<ConstNode>(ConstNode::type_string, Category::Producer);
        RegisterNodeType<PerlinNode>(PerlinNode::type_string, Category::Producer);

        // Modifier node types
        RegisterNodeType<BlendNode>(BlendNode::type_string, Category::Modifier);
        RegisterNodeType<InvertNode>(InvertNode::type_string, Category::Modifier);
        RegisterNodeType<ScaleBiasNode>(ScaleBiasNode::type_string, Category::Modifier);
        RegisterNodeType<SelectNode>(SelectNode::type_string, Category::Modifier);
        RegisterNodeType<StrataNode>(StrataNode::type_string, Category::Modifier);

        // Consumer node types
        RegisterNodeType<GenerateMeshNode>(GenerateMeshNode::type_string, Category::Consumer);
        RegisterNodeType<PreviewNode>(PreviewNode::type_string, Category::Consumer);
    }

    void NoiseNode_Factory::DrawMenuContent(ui::NG_Canvas& canvas, const ImVec2& popupPos)
    {
        const size_t total = m_categories.size();
        size_t count{ 0 };

        for (auto& [category, types] : m_categories)
        {
            for (auto& type : types)
            {
                if (ImGui::MenuItem(type.c_str()))
                    canvas.AddNode(std::move(CreateNode(type, popupPos)));
            }

            // Skip the separator the the last category.
            if (++count < total)
                ImGui::Separator();
        }
    }
}