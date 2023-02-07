#pragma once
#include "systems/scene.h"
#include "editor/imgui-widgets.h"
#include "editor/heightmap.h"

namespace cyb::editor
{
    struct TerrainMeshDesc
    {
        float size = 1000.0f;           // Terrain size in meters
        float maxAltitude = 160.0f;     // Peak height in meters
        float minAltitude = -22.0f;     // Lowest terrain point
        uint32_t numChunks = 8;         // Devide terrain in numChunks^2 seperate scene objects

        float maxError = 0.008f;
        uint32_t maxTriangles = 0;
        uint32_t maxVertices = 0;

        Heightmap* m_heightmap;
        Heightmap* m_moisturemap;
    };

    struct TerrainMesh
    {
        struct Chunk
        {
            float chunkSize;
            float chunkResolution;
            XMUINT2 bitmapOffset;

            std::vector<XMFLOAT3> vertices;
            std::vector<XMFLOAT3> colors;
            std::vector<uint32_t> indices;
        };

        TerrainMeshDesc desc;
        std::vector<Chunk> chunks;

        ////
        std::vector<XMFLOAT3> points;
        std::vector<XMINT3> triangles;
        ////

        //---- scene data ----
        ecs::Entity materialID;
        ecs::Entity groupID;
    };

    //=============================================================
    //  TerrainGenerator GUI
    //=============================================================

    void SetTerrainGenerationParams(const TerrainMeshDesc* params);

    class TerrainGenerator
    {
    public:
        enum class Map
        {
            Height,
            Moisture,
            Color
        };

        TerrainGenerator();
        void DrawGui(ecs::Entity selected_entity);
        const graphics::Texture* GetTerrainMapTexture(Map type) const;

    private:
        bool m_initialized = false;

        HeightmapDesc m_heightmapDesc;
        HeightmapDesc m_moisturemapDesc;
        NoiseDesc m_moisturemapNoise;
        Heightmap m_heightmap;
        Heightmap m_moisturemap;
        std::vector<uint32_t> m_colormap;
        graphics::Texture m_heightmapTex;
        graphics::Texture m_moisturemapTex;
        graphics::Texture m_colormapTex;
        ImGradient m_biomeColorBand;
        ImGradient m_moistureBiomeColorBand;

        TerrainMeshDesc m_meshDesc;
        TerrainMesh m_mesh;

        Map m_selectedMapType = Map::Height;
        bool m_drawChunkLines = false;
        bool m_useMoistureMap = false;
        
        void ResetParams();
        void DrawChunkLines(const ImVec2& drawStartPos) const;
        void DrawMeshTriangles(const ImVec2& drawStartPos) const;

        void UpdateHeightmap();
        void UpdateTextures();
        void UpdateHeightmapAndTextures();

        void GenerateTerrainMesh();
    };
}