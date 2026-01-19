#pragma once
#include "systems/scene.h"
#include "editor/heightmap.h"
#include "editor/widgets.h"

namespace cyb::editor
{
    using ImageGenPinSignature = float(float, float);
    using ImageGenInputPin = std::shared_ptr<ui::detail::NG_InputPin<ImageGenPinSignature>>;

    class PerlinNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Perlin Noise";
        using Signature = ImageGenPinSignature;

        PerlinNode();
        virtual ~PerlinNode() = default;
        void DisplayContent() override;

    private:
        noise2::PerlinNoiseParams m_param{};
    };

    class CellularNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Cellular Noise";
        using Signature = ImageGenPinSignature;

        CellularNode();
        virtual ~CellularNode() = default;
        void DisplayContent() override;

    private:
        noise2::CellularNoiseParams m_param{};
    };

    class ConstNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Const Value";
        using Signature = ImageGenPinSignature;

        ConstNode();
        virtual ~ConstNode() = default;
        void DisplayContent() override;

    private:
        float m_value{ 1.0f };
    };

    class BlendNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Blend";
        using Signature = ImageGenPinSignature;

        BlendNode();
        virtual ~BlendNode() = default;
        void DisplayContent() override;

    private:
        ImageGenInputPin m_inputA;
        ImageGenInputPin m_inputB;
        float m_alpha{ 0.5f };
    };

    class InvertNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Invert";
        using Signature = ImageGenPinSignature;

        InvertNode();
        virtual ~InvertNode() = default;

    private:
        ImageGenInputPin m_input;
    };

    class ScaleBiasNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "ScaleBias";
        using Signature = ImageGenPinSignature;

        ScaleBiasNode();
        virtual ~ScaleBiasNode() = default;
        void DisplayContent() override;

    private:
        ImageGenInputPin m_input;
        float m_scale{ 1.0f };
        float m_bias{ 0.0f };
    };

    class ModulateNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Strata";
        using Signature = ImageGenPinSignature;

        ModulateNode();
        virtual ~ModulateNode() = default;
        void DisplayContent() override;

    private:
        ImageGenInputPin m_input;
        float m_strata{ 5.0f };
    };

    class SelectNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Select";
        using Signature = ImageGenPinSignature;

        SelectNode();
        virtual ~SelectNode() = default;
        void DisplayContent() override;

    private:
        ImageGenInputPin m_inputA;
        ImageGenInputPin m_inputB;
        ImageGenInputPin m_inputControl;
        float m_threshold{ 0.5 };
        float m_edgeFalloff{ 0.0 };

    };

    class PreviewNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Preview";
        using Signature = ImageGenPinSignature;

        PreviewNode();
        virtual ~PreviewNode() = default;
        void UpdatePreview();
        void Update() override;
        void DisplayContent() override;

    private:
        ImageGenInputPin m_inputPin;
        bool m_autoUpdate = true;
        uint32_t m_previewSize = 128;   // Used as both width and height
        float m_lastPreviewGenerationTime = 0.0f;
        float m_freqScale = 8.0f;
        rhi::Texture m_texture;
    };

    class GenerateMeshNode : public ui::NG_Node
    {
    public:
        static constexpr std::string_view typeString = "Generate Mesh";
        using Signature = ImageGenPinSignature;

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

        ImageGenInputPin m_input;
        float m_generationTime{ 0.0f };
        int m_chunkExpand{ 2 };
        int m_chunkSize{ 256 };             // Chunk size in meters
        float m_maxError{ 0.004f };
        float m_minMeshAltitude{ -22.0f };  // Min altidude in meters
        float m_maxMeshAltitude{ 200.0f };  // Max altitude in meters
        ui::Gradient m_biomeColorBand;

        jobsystem::Context m_jobContext;
        ecs::Entity m_terrainGroupID{ ecs::INVALID_ENTITY };
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

#if 0

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

        // have a separate preview generation jobsystem to be able to wait for
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
#endif
}