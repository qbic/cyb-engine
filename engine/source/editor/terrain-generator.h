#pragma once
#include "systems/scene.h"
#include "editor/heightmap.h"
#include "editor/widgets.h"

namespace cyb::editor
{
    struct TerrainMeshDesc
    {
        float size = 1000.0f;           // terrain size in meters per chunk
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
        void CreatePreviewImage();

        void CreateTerrainAtOffset(const XMINT2 offset);
        void GenerateTerrainMesh();

    private:
        // have a seperate preview jobsystem to be able to wait for
        // previous render without stalling other jobsystems
        jobsystem::Context previewGenContext;
        int32_t previewResolution = 256;
        XMINT2 previewOffset = XMINT2(0, 0);
        graphics::Texture previewTex;

        jobsystem::Context jobContext;
        HeightmapGenerator heightmap;
        ui::Gradient m_biomeColorBand;
        TerrainMeshDesc m_meshDesc;
        ecs::Entity groundMateral = ecs::INVALID_ENTITY;
        ecs::Entity rockMatereal = ecs::INVALID_ENTITY;
    };
}