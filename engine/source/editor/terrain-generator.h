#pragma once
#include "systems/scene.h"
#include "core/noise.h"
#include "editor/imgui-widgets.h"

namespace cyb::editor
{
    enum class HeightmapCombineType
    {
        Mul,
        Lerp
    };

    struct NoiseDesc
    {
        NoiseType noiseType = NoiseType::Perlin;
        uint32_t seed = 0;              // Noise function seed value
        float frequency = 5.5f;         // Noise function frequency
        uint32_t octaves = 4;           // Fractal Brownian Motion (FBM) octaves
        NoiseStrataOp strataOp = NoiseStrataOp::None;
        float strata = 5.0f;            // Strata amount
        CellularReturnType cellularReturnType = CellularReturnType::Distance;
    };

    struct HeightmapDesc
    {
        uint32_t width = 512;
        uint32_t height = 512;
    };

    struct Heightmap
    {
        NoiseGenerator noiseGen;
        HeightmapDesc desc;
        std::atomic<float> maxN;
        std::atomic<float> minN;
        std::vector<float> image;       // Bitmap in 1ch 32-bit floating point format

        float GetHeightAt(uint32_t x, uint32_t y) const;
        float GetHeightAt(const XMINT2& p) const;
        std::pair<XMINT2, float> FindCandidate(const XMINT2 p0, const XMINT2 p1, const XMINT2 p2) const;
    };

    struct TerrainMeshDesc
    {
        float size = 1000.0f;           // Terrain size in meters
        float maxAltitude = 160.0f;     // Peak height in meters
        float minAltitude = -22.0f;     // Lowest terrain point
        uint32_t mapResolution = 512;   // Terrain and image resolutions, higher = more triangles
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
        NoiseDesc m_heightmapNoise1;
        NoiseDesc m_heightmapNoise2;
        HeightmapCombineType m_combineType = HeightmapCombineType::Lerp;
        float m_heightmapNoiseMix = 0.5f;
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
        bool m_drawTriangles = false;
        bool m_useMoistureMap = false;
        
        void ResetParams();
        void DrawChunkLines(const ImVec2& drawStartPos) const;
        void DrawMeshTriangles(const ImVec2& drawStartPos) const;

        void UpdateAllMaps();   // also updates colormap
        void UpdateColormap(jobsystem::Context& ctx);
        void UpdateColormapAndTextures();
        void UpdateTextures();
        void UpdateBitmapsAndTextures();

        void UpdateTerrainMesh();
        void GenerateTerrainMesh();
    };
}