#pragma once
#include "systems/scene.h"
#include "core/noise.h"
#include "editor/imgui-widgets.h"

namespace cyb::editor
{
    struct TerrainBitmapDesc
    {
        enum class Strata 
        {
            None,
            SharpSub,
            SharpAdd,
            Quantize,
            Smooth
        };

        uint32_t width = 512;
        uint32_t height = 512;
        bool normalize = true;          // Normalize all values in bitmap to [0..1] range
        uint32_t seed = 0;              // Noise function seed value
        float frequency = 5.5f;         // Noise function frequency
        uint32_t octaves = 6;           // Fractal Brownian Motion (FBM) octaves
        NoiseGenerator::Interpolation interp = NoiseGenerator::Interpolation::Quintic;
        Strata strataFunc = Strata::None;
        float strata = 5.0f;            // Strata amount
    };

    struct TerrainBitmap
    {
        TerrainBitmapDesc desc;
        std::atomic<float> maxN;
        std::atomic<float> minN;
        std::vector<float> image;       // Bitmap in 1ch 32-bit floating point format
    };

    struct TerrainMeshDesc
    {
        float size = 1000.0f;           // Terrain size in meters
        float maxAltitude = 120.0f;     // Peak height in meters
        float minAltitude = -22.0f;     // Lowest terrain point
        uint32_t mapResolution = 512;   // Terrain and image resolutions, higher = more triangles
        uint32_t numChunks = 8;         // Devide terrain in numChunks^2 seperate scene objects

        TerrainBitmap* m_heightmap;
        TerrainBitmap* m_moisturemap;
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

        //---- scene data ----
        ecs::Entity materialID;
        ecs::Entity groupID;
    };

    void CreateTerrainBitmap(jobsystem::Context& ctx, const TerrainBitmapDesc& desc, TerrainBitmap& bitmap);
    void NormalizeTerrainBitmapValues(jobsystem::Context& ctx, TerrainBitmap& bitmap);
    void CreateTerrainColormap(jobsystem::Context& ctx, const TerrainBitmap& height, const TerrainBitmap& moisture, std::vector<uint32_t>& color);
    void CreateTerrainMesh(const TerrainMeshDesc& desc, TerrainMesh& terrain);

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
        TerrainMeshDesc m_meshDesc;
        TerrainBitmapDesc m_heightmapDesc;
        TerrainBitmapDesc m_moisturemapDesc;

        TerrainBitmap m_heightmap;
        TerrainBitmap m_moisturemap;
        std::vector<uint32_t> m_colormap;

        graphics::Texture m_heightmapTex;
        graphics::Texture m_moisturemapTex;
        graphics::Texture m_colormapTex;

        Map m_selectedMapType = Map::Height;
        bool m_drawChunkLines = false;

        bool m_useMoistureMap = false;
        ImGradient m_biomeColorBand;
        ImGradient m_moistureBiomeColorBand;
        
        void DrawChunkLines(const ImVec2& drawStartPos) const;
        void UpdateBitmaps();   // also updates colormap
        void UpdateColormap(jobsystem::Context& ctx);
        void UpdateColormapAndTextures();
        void UpdateBitmapTextures();
        void UpdateBitmapsAndTextures();
    };
}