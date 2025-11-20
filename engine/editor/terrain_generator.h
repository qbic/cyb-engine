#pragma once
#include "systems/scene.h"
#include "editor/heightmap.h"
#include "editor/widgets.h"

namespace cyb::editor
{
    class PerlinNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Perlin Noise";

        PerlinNode();
        virtual ~PerlinNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Perlin m_noise;
    };

    class CellularNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Cellular Noise";

        CellularNode();
        virtual ~CellularNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Cellular m_noise;
    };

    class ConstNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Const Value";

        ConstNode();
        virtual ~ConstNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Const m_const;
    };

    class BlendNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Blend";

        BlendNode();
        virtual ~BlendNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Blend m_blend;
    };

    class InvertNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Invert";

        InvertNode();
        virtual ~InvertNode() = default;

    private:
        noise2::NoiseNode_Invert m_inv;
    };

    class ScaleBiasNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "ScaleBias";

        ScaleBiasNode();
        virtual ~ScaleBiasNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_ScaleBias m_scaleBias;
    };

    class StrataNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Strata";

        StrataNode();
        virtual ~StrataNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Strata m_strata;
    };

    class SelectNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Select";

        SelectNode();
        virtual ~SelectNode() = default;
        void DisplayContent() override;

    private:
        noise2::NoiseNode_Select m_select;
    };

    class PreviewNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Preivew";

        PreviewNode();
        virtual ~PreviewNode() = default;
        void UpdatePreview();
        void Update() override;
        void DisplayContent() override;

    private:
        bool m_autoUpdate = true;
        uint32_t m_previewSize = 128;   // Used as both width and height
        float m_lastPreviewGenerationTime = 0.0f;
        float m_freqScale = 8.0f;
        rhi::Texture m_texture;
        noise2::NoiseNode* m_input = nullptr;
    };

    class GenerateMeshNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Generate Mesh";

        GenerateMeshNode();
        virtual ~GenerateMeshNode() = default;
        void DisplayContent() override;

        void GenerateTerrainMesh();

    private:
        struct Chunk
        {
            int32_t x{ 0 };
            int32_t z{ 0 };

            constexpr bool operator==(const Chunk& other) const
            {
                return (x == other.x) && (z == other.z);
            }
            inline size_t GetHash() const
            {
                return ((std::hash<int>()(x) ^ (std::hash<int>()(z) << 1)) >> 1);
            }
        };

        struct ChunkData
        {
            ecs::Entity entity{ ecs::INVALID_ENTITY };
            scene::Scene scene;
        };

        struct ChunkHash
        {
            inline size_t operator()(const Chunk& chunk) const
            {
                return chunk.GetHash();
            }
        };

        int m_chunkExpand{ 2 };
        int m_chunkSize{ 256 };             // Chunk size in meters
        float m_maxError{ 0.004f };
        float m_minMeshAltitude{ -22.0f };  // Min altidude in meters
        float m_maxMeshAltitude{ 200.0f };  // Max altitude in meters
        ui::Gradient m_biomeColorBand;

        jobsystem::Context m_jobContext;
        ecs::Entity m_terrainGroupID{ ecs::INVALID_ENTITY };
        noise2::NoiseNode* m_input{ nullptr };
    };

    class NoiseNode_Factory : public ui::NG_Factory
    {
    public:
        NoiseNode_Factory();
        virtual ~NoiseNode_Factory() = default;
        void DrawMenuContent(ui::NG_Canvas& canvas, const ImVec2& popupPos) override;

    private:
        enum class Category { Producer, Modifier, Consumer };

        template <typename T>
        void RegisterNodeType(const std::string_view& name, Category category)
        {
            auto& container = m_categories[category];
            container.push_back(name.data());
            NG_Factory::RegisterFactoryFunction<T>(name);
        }

        std::map<Category, std::vector<std::string>> m_categories;
    };

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////

    struct TerrainMeshDesc
    {
        int32_t size = 256;             // terrain size in meters per chunk
        float maxAltitude = 200.0f;     // peak height in meters
        float minAltitude = -22.0f;     // lowest terrain point
        int32_t chunkExpand = 2;        // number of chunks to expand in every direction from center

        float maxError = 0.004f;
        uint32_t maxTriangles = 0;
        uint32_t maxVertices = 0;
    };

    //=============================================================
    //  TerrainGenerator GUI
    //=============================================================

    class TerrainGenerator
    {
    public:
        TerrainGenerator();
        void DrawGui(ecs::Entity selected_entity);

    private:
        void SetDefaultHeightmapValues();
        void UpdatePreview();

        void RemoveTerrainData();
        void GenerateTerrainMesh();

    private:
        struct Chunk
        {
            int32_t x, z;
            constexpr bool operator==(const Chunk& other) const
            {
                return (x == other.x) && (z == other.z);
            }
            inline size_t GetHash() const
            {
                return ((std::hash<int>()(x) ^ (std::hash<int>()(z) << 1)) >> 1);
            }
        };

        struct ChunkHash
        {
            inline size_t operator()(const Chunk& chunk) const
            {
                return chunk.GetHash();
            }
        };

        struct ChunkData
        {
            ecs::Entity entity = ecs::INVALID_ENTITY;
            scene::Scene scene;
        };

        // have a seperate preview generation jobsystem to be able to wait for
        // previous preview update without stalling other jobsystems
        jobsystem::Context m_updatePreviewCtx;
        XMINT2 m_previewOffset = XMINT2(0, 0);
        rhi::Texture m_preview;

        jobsystem::Context jobContext;
        HeightmapGenerator heightmap;
        ui::Gradient m_biomeColorBand;
        TerrainMeshDesc m_meshDesc;

        std::atomic_bool cancelTerrainGen = false;
        std::unordered_map<Chunk, ChunkData, ChunkHash> chunks;
        Chunk centerChunk = {};

        ecs::Entity terrainEntity = ecs::INVALID_ENTITY;
        ecs::Entity groundMateral = ecs::INVALID_ENTITY;
        ecs::Entity rockMatereal = ecs::INVALID_ENTITY;
    };
}