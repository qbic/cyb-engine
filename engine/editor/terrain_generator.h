#pragma once
#include "systems/scene.h"
#include "editor/heightmap.h"
#include "editor/widgets.h"

namespace cyb::editor
{
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